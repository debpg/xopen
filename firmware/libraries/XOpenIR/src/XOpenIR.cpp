#include "XOpenIR.h"

/* ------------------------------------------------------------------ *
 *  Frame
 * ------------------------------------------------------------------ */

bool xopenParity(uint32_t data, uint8_t len)
{
    uint8_t ones = 0;
    for (uint8_t i = 0; i < len; i++) {
        if ((data >> i) & 0x1) ones++;
    }
    return (ones % 2);
}

static uint32_t xopenBuildSys(uint8_t command, uint8_t arg)
{
    uint32_t packet = 0;          /* team 0 = comando di sistema */
    packet <<= 5;
    packet |= command & 0x1F;
    packet <<= 8;
    packet |= arg;

    packet <<= 1;
    packet |= xopenParity(packet >> 1, 16);
    return packet;
}

void xopenDecode(uint32_t raw, XOpenFrame &frame)
{
    uint32_t payload = (raw >> 1) & 0xFFFF;
    bool     parity  = raw & 0x1;

    frame.raw   = raw;
    frame.valid = (parity == xopenParity(payload, 16));

    uint8_t msb = (payload >> 8) & 0xFF;
    frame.damage   = payload & 0xFF;
    frame.player   = msb & 0x1F;
    frame.team     = (msb >> 5) & 0x07;
    frame.isSystem = (frame.team == 0);
    frame.command  = frame.player;
    frame.arg      = frame.damage;
}

/* ------------------------------------------------------------------ *
 *  Trasmettitore
 * ------------------------------------------------------------------ */

XOpenIRSend::XOpenIRSend(uint8_t duty) : _duty(duty ? duty : 3)
{
}

void XOpenIRSend::begin()
{
    pinMode(XOPENIR_TX_PIN, OUTPUT);
    digitalWrite(XOPENIR_TX_PIN, LOW);

    /* PWM phase-correct con TOP = OCR2A, prescaler 1.
       f = F_CPU / (2 * OCR2A) = 40 kHz.                       */
    TCCR2A = _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS20);
    OCR2A  = (F_CPU / 2000 / 40);
    OCR2B  = (F_CPU / 2000 / 40) / _duty;
    carrierOff();
}

void XOpenIRSend::sendRaw(uint32_t frame)
{
    /* Il frame dura 15..32 ms e viene generato a colpi di busy wait: va
       emesso con le interruzioni disabilitate, altrimenti la ISR di
       millis() allarga i mark quel tanto che basta a far sbagliare la
       classificazione al ricevitore. Effetto collaterale noto: millis()
       perde la durata del frame ad ogni invio. */
    noInterrupts();

    carrierOn();
    delayMicroseconds(XOPENIR_MARK_HEADER);
    carrierOff();
    delayMicroseconds(XOPENIR_SPACE);

    for (int8_t i = XOPENIR_BITS - 1; i >= 0; i--) {
        carrierOn();
        if ((frame >> i) & 0x1) {
            delayMicroseconds(XOPENIR_MARK_ONE);
        } else {
            delayMicroseconds(XOPENIR_MARK_ZERO);
        }
        carrierOff();
        delayMicroseconds(XOPENIR_SPACE);
    }

    interrupts();
}

void XOpenIRSend::sendSys(uint8_t command, uint8_t arg, uint8_t repeat)
{
    uint32_t frame = xopenBuildSys(command, arg);

    for (uint8_t i = 0; i < repeat; i++) {
        if (i) delay(XOPENIR_REPEAT_GAP_MS);
        sendRaw(frame);
    }
}

/* ------------------------------------------------------------------ *
 *  Ricevitore
 * ------------------------------------------------------------------ */

XOpenIRRecv *XOpenIRRecv::instance = NULL;

static void xopenRecvIsr()
{
    if (XOpenIRRecv::instance) XOpenIRRecv::instance->handleEdge();
}

XOpenIRRecv::XOpenIRRecv(uint8_t pin)
{
    _pin    = pin;
    _ready  = false;
    _frame  = 0;
    _lastEdge = 0;
    resetDecoder();
}

void XOpenIRRecv::begin()
{
    pinMode(_pin, INPUT);          /* il TSOP ha gia' il pull-up interno */
    _port = portInputRegister(digitalPinToPort(_pin));
    _mask = digitalPinToBitMask(_pin);

    instance = this;
    attachInterrupt(digitalPinToInterrupt(_pin), xopenRecvIsr, CHANGE);
}

void XOpenIRRecv::resetDecoder()
{
    _value  = 0;
    _bits   = 0;
    _header = false;
}

void XOpenIRRecv::handleEdge()
{
    uint32_t now = micros();
    /* Uscita del TSOP attiva bassa: livello basso = portante presente. */
    bool mark = ((*_port & _mask) == 0);

    if (mark) {
        /* Inizio di un mark. Se dall'ultimo fronte e' passata troppa roba
           il frame precedente e' monco: si riparte da capo. */
        if (now - _lastEdge > XOPENIR_RECV_GAP_RESET) resetDecoder();
        _markStart = now;
        _lastEdge  = now;
        return;
    }

    _lastEdge = now;

    if (_ready) return;            /* frame gia' pronto, non ancora letto */

    uint32_t len = now - _markStart;

    if (!_header) {
        if (len > XOPENIR_RECV_HEADER_MIN && len < XOPENIR_RECV_TOO_LATE) {
            _header = true;
            _value  = 0;
            _bits   = 0;
        }
        return;
    }

    if (len > XOPENIR_RECV_TOO_LATE) {
        resetDecoder();
    } else if (len > XOPENIR_RECV_ONE_MIN) {
        _value = (_value << 1) | 0x1;
        _bits++;
    } else if (len > XOPENIR_RECV_ZERO_MIN) {
        _value = (_value << 1);
        _bits++;
    } else {
        resetDecoder();
        return;
    }

    if (_bits == XOPENIR_BITS) {
        _frame = _value;
        _ready = true;
        resetDecoder();
    }
}

bool XOpenIRRecv::read(XOpenFrame &frame)
{
    uint32_t raw;

    noInterrupts();
    raw    = _frame;
    _ready = false;
    interrupts();

    xopenDecode(raw, frame);
    return frame.valid;
}

/* ------------------------------------------------------------------ *
 *  Ricevitore multicanale
 * ------------------------------------------------------------------ */

XOpenIRMulti *XOpenIRMulti::instance = NULL;

ISR(TIMER2_COMPA_vect)
{
    if (XOpenIRMulti::instance) XOpenIRMulti::instance->handleTick();
}

XOpenIRMulti::XOpenIRMulti(uint8_t count)
{
    _count = (count > XOPENIR_MULTI_MAX) ? XOPENIR_MULTI_MAX : count;
    _tick  = 0;

    for (uint8_t ch = 0; ch < XOPENIR_MULTI_MAX; ch++) {
        _ready[ch]    = false;
        _frame[ch]    = 0;
        _lastEdge[ch] = 0;
        _markStart[ch] = 0;
        resetChannel(ch);
    }
}

void XOpenIRMulti::resetChannel(uint8_t ch)
{
    _value[ch]  = 0;
    _bits[ch]   = 0;
    _header[ch] = false;
}

void XOpenIRMulti::begin()
{
    /* A0..A5 come ingressi digitali. I TSOP hanno il pull-up interno,
       quindi INPUT e basta: un INPUT_PULLUP qui sopra non farebbe danni
       ma non serve. */
    for (uint8_t ch = 0; ch < _count; ch++) pinMode(A0 + ch, INPUT);

    /* Stato iniziale degli ingressi: senza questo il primo campionamento
       vedrebbe un fronte inesistente su ogni canale. */
    _lastPins = PINC;

    instance = this;

    /* Timer2 in CTC, prescaler 8: un'interruzione ogni XOPENIR_SAMPLE_US.
       Con 50 us e F_CPU 16 MHz -> OCR2A = 99. */
    noInterrupts();
    TCCR2A = _BV(WGM21);
    TCCR2B = _BV(CS21);
    OCR2A  = (uint8_t)((F_CPU / 8UL / (1000000UL / XOPENIR_SAMPLE_US)) - 1);
    TCNT2  = 0;
    TIMSK2 = _BV(OCIE2A);
    interrupts();
}

void XOpenIRMulti::handleTick()
{
    _tick++;

    /* Una sola lettura per tutti i canali: sono tutti su PORTC. */
    uint8_t pins    = PINC;
    uint8_t changed = pins ^ _lastPins;

    /* Nella stragrande maggioranza dei campionamenti non e' cambiato
       niente, e questa ISR finisce qui. Il caso pesante - cinque fronti
       nello stesso campionamento - capita davvero, perche' i sensori
       vedono tutti lo stesso fascio; se dovesse sforare i
       XOPENIR_SAMPLE_US non si perdono campionamenti, il compare match
       resta pendente e la ISR riparte subito dopo. */
    if (!changed) return;

    _lastPins = pins;

    for (uint8_t ch = 0; ch < _count; ch++) {
        if (!(changed & (1 << ch))) continue;

        /* Uscita del TSOP attiva bassa: livello basso = portante presente. */
        bool mark = ((pins & (1 << ch)) == 0);

        if (mark) {
            /* Inizio mark. Se dall'ultimo fronte e' passato troppo tempo,
               il frame in corso era monco. */
            if ((uint16_t)(_tick - _lastEdge[ch]) > XOPENIR_MULTI_MAX_MARK_TICKS)
                resetChannel(ch);
            _markStart[ch] = _tick;

        } else if (!_ready[ch]) {
            uint16_t ticks = (uint16_t)(_tick - _markStart[ch]);

            if (ticks > XOPENIR_MULTI_MAX_MARK_TICKS) {
                resetChannel(ch);
            } else {
                uint16_t len = ticks * XOPENIR_SAMPLE_US;

                if (!_header[ch]) {
                    if (len > XOPENIR_RECV_HEADER_MIN && len < XOPENIR_RECV_TOO_LATE) {
                        _header[ch] = true;
                        _value[ch]  = 0;
                        _bits[ch]   = 0;
                    }

                } else if (len > XOPENIR_RECV_TOO_LATE) {
                    resetChannel(ch);

                } else if (len > XOPENIR_RECV_ONE_MIN) {
                    _value[ch] = (_value[ch] << 1) | 0x1;
                    _bits[ch]++;

                } else if (len > XOPENIR_RECV_ZERO_MIN) {
                    _value[ch] = (_value[ch] << 1);
                    _bits[ch]++;

                } else {
                    resetChannel(ch);
                }

                if (_bits[ch] == XOPENIR_BITS) {
                    _frame[ch] = _value[ch];
                    _ready[ch] = true;
                    resetChannel(ch);
                }
            }
        }

        _lastEdge[ch] = _tick;
    }
}

bool XOpenIRMulti::available(uint8_t ch) const
{
    return (ch < _count) && _ready[ch];
}

bool XOpenIRMulti::read(uint8_t ch, XOpenFrame &frame)
{
    uint32_t raw;

    if (ch >= _count) return false;

    noInterrupts();
    raw        = _frame[ch];
    _ready[ch] = false;
    interrupts();

    xopenDecode(raw, frame);
    return frame.valid;
}

/* ------------------------------------------------------------------ *
 *  Croce di taratura
 * ------------------------------------------------------------------ */

XOpenIRCross::XOpenIRCross() : _multi(XOPEN_CROSS_CHANNELS)
{
    _mask = _team = _player = _damage = 0;
    _hit  = false;
    _evMask = 0;
    _evMs = 0;
    _ledMask = 0;
    _ledUntil = 0;
    _map[0] = '\0';
    reset();
}

void XOpenIRCross::begin()
{
    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) {
        pinMode(XOPEN_CROSS_LED_PIN + ch, OUTPUT);
    }
    writeLeds(0);
    _multi.begin();
}

void XOpenIRCross::writeLeds(uint8_t mask)
{
    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) {
        digitalWrite(XOPEN_CROSS_LED_PIN + ch, (mask >> ch) & 0x1);
    }
}

void XOpenIRCross::buildMap()
{
    /* .X.|XXX|.X.  - riga alta, riga centrale, riga bassa */
    _map[0] = '.';
    _map[1] = (_mask & (1 << XOPEN_CROSS_UP))     ? 'X' : '.';
    _map[2] = '.';
    _map[3] = '|';
    _map[4] = (_mask & (1 << XOPEN_CROSS_LEFT))   ? 'X' : '.';
    _map[5] = (_mask & (1 << XOPEN_CROSS_CENTER)) ? 'X' : '.';
    _map[6] = (_mask & (1 << XOPEN_CROSS_RIGHT))  ? 'X' : '.';
    _map[7] = '|';
    _map[8] = '.';
    _map[9] = (_mask & (1 << XOPEN_CROSS_DOWN))   ? 'X' : '.';
    _map[10] = '.';
    _map[11] = '\0';
}

void XOpenIRCross::loop()
{
    uint32_t now = millis();

    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) {
        if (!_multi.available(ch)) continue;

        XOpenFrame f;
        /* Parita' errata: il frame e' arrivato rovinato, si butta. */
        if (!_multi.read(ch, f)) continue;
        /* Comandi di sistema e frame di autotest non sono colpi. */
        if (f.isSystem || f.damage == 0) continue;

        _evMask |= (1 << ch);
        _evMs    = now;
        _team    = f.team;
        _player  = f.player;
        _damage  = f.damage;

        _ledMask |= (1 << ch);
        writeLeds(_ledMask);
        _ledUntil = now + XOPEN_CROSS_LED_MS;
    }

    if (_evMask && (now - _evMs > XOPEN_CROSS_WINDOW_MS)) {
        _mask   = _evMask;
        _evMask = 0;
        _shots++;
        for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) {
            if ((_mask >> ch) & 0x1) _chShots[ch]++;
        }
        buildMap();
        _hit = true;
    }

    if (_ledMask && now >= _ledUntil) {
        _ledMask = 0;
        writeLeds(0);
    }
}

bool XOpenIRCross::hit()
{
    if (!_hit) return false;
    _hit = false;
    return true;
}

uint8_t XOpenIRCross::sensors() const
{
    uint8_t n = 0;
    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) n += (_mask >> ch) & 0x1;
    return n;
}

uint16_t XOpenIRCross::sensorShots(uint8_t ch) const
{
    return (ch < XOPEN_CROSS_CHANNELS) ? _chShots[ch] : 0;
}

void XOpenIRCross::reset()
{
    _shots  = 0;
    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) _chShots[ch] = 0;
}

void XOpenIRCross::report(Print &out) const
{
    out.println();
    out.print(F("colpi: "));
    out.println(_shots);

    if (_shots == 0) return;

    uint8_t pct[XOPEN_CROSS_CHANNELS];
    for (uint8_t ch = 0; ch < XOPEN_CROSS_CHANNELS; ch++) {
        pct[ch] = (uint8_t)(((uint32_t)_chShots[ch] * 100UL) / _shots);
    }

    char line[24];
    snprintf(line, sizeof(line), "       %3u%%", pct[XOPEN_CROSS_UP]);
    out.println(line);
    snprintf(line, sizeof(line), " %3u%%  %3u%%  %3u%%",
             pct[XOPEN_CROSS_LEFT], pct[XOPEN_CROSS_CENTER], pct[XOPEN_CROSS_RIGHT]);
    out.println(line);
    snprintf(line, sizeof(line), "       %3u%%", pct[XOPEN_CROSS_DOWN]);
    out.println(line);
    out.println();
}

/*
 * XOpenIR - libreria minimale per il link infrarosso X-TAG su Arduino Nano.
 *
 * Portante 40 kHz, frame da 17 bit (header + 16 bit di payload + parita' pari).
 *
 *   bit  16 15 14 | 13 12 11 10  9 | 8 7 6 5 4 3 2 1 | 0
 *        ---------+----------------+-----------------+---
 *          TEAM   |     NUMBER     |      DATA       | P
 *           (3)   |       (5)      |       (8)       |(1)
 *
 * TEAM 1..7 -> colpo (DATA = danno 1..255).
 * TEAM 0    -> comando di sistema (NUMBER = comando, DATA = argomento).
 *
 * La libreria sa DECODIFICARE i colpi (serve al bersaglio e alla croce)
 * ma non sa generarli: si trasmettono solo comandi di supporto, cioe'
 * respawn, munizioni e cura.
 *
 * Licenza: MIT. Vedi LICENSE nel repository xopen.
 */
#ifndef XOPENIR_H
#define XOPENIR_H

#include <Arduino.h>

#if !defined(__AVR_ATmega328P__) && !defined(__AVR_ATmega328__) && !defined(__AVR_ATmega168__)
#warning "XOpenIR e' scritta e testata per ATmega328P (Arduino Nano / Uno / Pro Mini)."
#endif

/* ------------------------------------------------------------------ *
 *  Temporizzazioni (microsecondi)
 * ------------------------------------------------------------------ */
#define XOPENIR_BITS            17
#define XOPENIR_CARRIER_HZ      40000UL

#define XOPENIR_MARK_HEADER     2350
#define XOPENIR_MARK_ONE        1150
#define XOPENIR_MARK_ZERO        550
#define XOPENIR_SPACE            550

/* Soglie di decodifica, identiche a quelle del firmware X-TAG. */
#define XOPENIR_RECV_HEADER_MIN 2200
#define XOPENIR_RECV_ONE_MIN     900
#define XOPENIR_RECV_ZERO_MIN    450
#define XOPENIR_RECV_TOO_LATE   3000
/* Se fra due fronti passa piu' di questo, il frame in corso e' perso. */
#define XOPENIR_RECV_GAP_RESET  5000

/* ------------------------------------------------------------------ *
 *  Comandi di supporto (TEAM == 0)
 *
 *  Solo quelli che servono al telecomando: respawn, munizioni, cura e
 *  test dei sensori.
 * ------------------------------------------------------------------ */
#define XOPENIR_SYS_HEAL              0x01  /* arg = punti vita da aggiungere  */
#define XOPENIR_SYS_ADMIN             0x09  /* arg = sotto-comando, sotto      */

#define XOPENIR_ADMIN_RESPAWN         0x04
#define XOPENIR_ADMIN_FULLAMMO        0x06
#define XOPENIR_ADMIN_SENSOR_TEST     0x15

/* Respawn e munizioni vanno mandati tre volte di fila perche' il visore
   li esegua: se ne arriva uno solo non succede niente, senza nessuna
   segnalazione. La pausa fra un frame e il successivo deve restare
   breve. La cura e il test sensori agiscono al primo frame. */
#define XOPENIR_ADMIN_REPEAT          3
#define XOPENIR_REPEAT_GAP_MS        60

/* ------------------------------------------------------------------ *
 *  Frame decodificato
 * ------------------------------------------------------------------ */
struct XOpenFrame {
    uint32_t raw;      /* i 17 bit come sono arrivati */
    bool     valid;    /* parita' corretta            */
    bool     isSystem; /* team == 0                   */
    uint8_t  team;     /* 1..7 (0 se comando)         */
    uint8_t  player;   /* 0..31                       */
    uint8_t  damage;   /* 1..255                      */
    uint8_t  command;  /* solo se isSystem            */
    uint8_t  arg;      /* solo se isSystem            */
};

/* Parita' pari sui primi len bit: 1 se il numero di bit a 1 e' dispari. */
bool xopenParity(uint32_t data, uint8_t len);

/* Decodifica: ritorna frame.valid == false se la parita' non torna. */
void xopenDecode(uint32_t raw, XOpenFrame &frame);

/* ------------------------------------------------------------------ *
 *  Trasmettitore
 *
 *  Usa il Timer2 in PWM phase-correct con TOP = OCR2A, quindi l'uscita
 *  della portante e' obbligatoriamente OC2B = D3 sul Nano.
 *  Occupare il Timer2 significa perdere tone() e il PWM su D3 e D11.
 * ------------------------------------------------------------------ */
#define XOPENIR_TX_PIN 3

class XOpenIRSend {
public:
    /* duty: divisore del duty cycle della portante. 3 = 1/3 (nominale,
       massima portata), 64 = 1/64 (modalita' "boost 0" a bassa potenza). */
    explicit XOpenIRSend(uint8_t duty = 3);

    void begin();

    /* I comandi di supporto, gia' con la ripetizione giusta. */
    void respawn()         { sendSys(XOPENIR_SYS_ADMIN, XOPENIR_ADMIN_RESPAWN,  XOPENIR_ADMIN_REPEAT); }
    void fullAmmo()        { sendSys(XOPENIR_SYS_ADMIN, XOPENIR_ADMIN_FULLAMMO, XOPENIR_ADMIN_REPEAT); }
    void heal(uint8_t hp)  { sendSys(XOPENIR_SYS_HEAL, hp); }
    void sensorTest()      { sendSys(XOPENIR_SYS_ADMIN, XOPENIR_ADMIN_SENSOR_TEST); }

private:
    uint8_t _duty;
    void sendSys(uint8_t command, uint8_t arg, uint8_t repeat = 1);
    void sendRaw(uint32_t frame);
    void carrierOn()  { TCCR2A |=  _BV(COM2B1); }
    void carrierOff() { TCCR2A &= ~_BV(COM2B1); }
};

/* ------------------------------------------------------------------ *
 *  Ricevitore a un canale
 *
 *  Un solo TSOP, collegato a un pin con interrupt esterno (D2 o D3 sul
 *  Nano). La decodifica avviene tutta nella ISR sui fronti; loop() si
 *  limita a raccogliere il frame completo.
 *
 *  Per piu' di due sensori serve XOpenIRMulti: gli interrupt esterni di
 *  un 328P sono due e basta.
 * ------------------------------------------------------------------ */
class XOpenIRRecv {
public:
    explicit XOpenIRRecv(uint8_t pin = 2);

    void begin();

    /* true se c'e' un frame da leggere. */
    bool available() const { return _ready; }

    /* Consuma il frame. Ritorna false se la parita' non torna: il frame
       e' arrivato rovinato e va semplicemente buttato. */
    bool read(XOpenFrame &frame);

    /* Chiamata dalla ISR: non usare direttamente. */
    void handleEdge();

    static XOpenIRRecv *instance;

private:
    uint8_t  _pin;
    volatile uint8_t *_port;
    uint8_t  _mask;

    volatile uint32_t _value;
    volatile uint32_t _markStart;
    volatile uint32_t _lastEdge;
    volatile uint8_t  _bits;
    volatile bool     _header;
    volatile bool     _ready;
    volatile uint32_t _frame;

    void resetDecoder();
};

/* ------------------------------------------------------------------ *
 *  Ricevitore multicanale
 *
 *  Fino a 6 TSOP indipendenti sui pin analogici A0..A5, usati come
 *  ingressi digitali. Niente interrupt sui fronti: il Timer2 campiona
 *  tutti gli ingressi ogni XOPENIR_SAMPLE_US e ogni canale ha la sua
 *  macchina a stati.
 *
 *  Perche' A0..A5 e non pin sparsi: stanno tutti su PORTC, quindi la ISR
 *  legge i sei ingressi con una singola istruzione invece di sei
 *  digitalRead(). Nella ISR non si chiama micros(): le lunghezze degli
 *  impulsi si contano in campionamenti.
 *
 *  IMPORTANTE - i canali sono davvero indipendenti. Il firmware X-TAG,
 *  che ha lo stesso schema sulle tre teste sensore dell'headset, fa il
 *  contrario: appena un ricevitore completa un frame gli altri smettono
 *  di essere campionati, perche' un solo colpo visto da tre sensori non
 *  deve contare tre volte il danno. Qui il danno non lo conta nessuno e
 *  sapere QUALI sensori sono stati coperti e' tutto il punto della
 *  misura, quindi quella de-duplica non c'e'.
 *
 *  Occupa il Timer2: non puo' convivere con XOpenIRSend, e niente
 *  tone() ne' analogWrite() su D3 e D11.
 * ------------------------------------------------------------------ */
#define XOPENIR_MULTI_MAX    6
#define XOPENIR_SAMPLE_US   50

/* Un mark piu' lungo di questo e' rumore o un frame monco: si riparte. */
#define XOPENIR_MULTI_MAX_MARK_TICKS 100

class XOpenIRMulti {
public:
    explicit XOpenIRMulti(uint8_t count = 5);

    void begin();

    bool available(uint8_t ch) const;
    /* Consuma il frame del canale. false = parita' errata, si butta. */
    bool read(uint8_t ch, XOpenFrame &frame);

    uint8_t count() const { return _count; }

    /* Chiamata dalla ISR del Timer2: non usare direttamente. */
    void handleTick();

    static XOpenIRMulti *instance;

private:
    uint8_t _count;

    volatile uint16_t _tick;
    volatile uint16_t _markStart[XOPENIR_MULTI_MAX];
    volatile uint16_t _lastEdge[XOPENIR_MULTI_MAX];
    volatile uint32_t _value[XOPENIR_MULTI_MAX];
    volatile uint32_t _frame[XOPENIR_MULTI_MAX];
    volatile uint8_t  _bits[XOPENIR_MULTI_MAX];
    volatile bool     _header[XOPENIR_MULTI_MAX];
    volatile bool     _ready[XOPENIR_MULTI_MAX];
    /* Stato precedente di tutti gli ingressi, per lavorare nella ISR solo
       sui canali che hanno davvero commutato. */
    volatile uint8_t  _lastPins;

    void resetChannel(uint8_t ch);
};

/* ------------------------------------------------------------------ *
 *  Croce di taratura
 *
 *  Il pezzo pronto all'uso: cinque TSOP su A0..A4, cinque LED su
 *  D8..D12, e tutto il contorno gia' fatto - accensione dei LED,
 *  raggruppamento dei frame di uno stesso colpo, conteggi.
 *
 *  Uno sketch completo sta in una decina di righe:
 *
 *      XOpenIRCross croce;
 *      void setup()  { Serial.begin(115200); croce.begin(); }
 *      void loop()   {
 *          croce.loop();
 *          if (croce.hit()) Serial.println(croce.map());
 *      }
 *
 *  Un colpo solo viene visto da piu' sensori a qualche millisecondo di
 *  distanza: loop() li raccoglie e li presenta come un unico colpo.
 * ------------------------------------------------------------------ */
#define XOPEN_CROSS_CENTER   0
#define XOPEN_CROSS_UP       1
#define XOPEN_CROSS_DOWN     2
#define XOPEN_CROSS_LEFT     3
#define XOPEN_CROSS_RIGHT    4
#define XOPEN_CROSS_CHANNELS 5

/* Primo pin dei LED: i cinque occupano D8..D12. */
#define XOPEN_CROSS_LED_PIN  8
/* Per quanto restano accesi dopo un colpo. */
#define XOPEN_CROSS_LED_MS   400
/* Entro questa finestra i frame appartengono allo stesso colpo. */
#define XOPEN_CROSS_WINDOW_MS 80

class XOpenIRCross {
public:
    XOpenIRCross();

    void begin();
    /* Da chiamare ad ogni giro di loop(): fa tutto il lavoro. */
    void loop();

    /* true una volta sola, quando un colpo e' stato raccolto per intero. */
    bool hit();

    /* Il colpo appena raccolto. */
    uint8_t mask()    const { return _mask; }    /* bit 0..4, vedi le costanti */
    uint8_t sensors() const;                     /* quanti sensori coperti     */
    uint8_t team()    const { return _team; }
    uint8_t player()  const { return _player; }
    uint8_t damage()  const { return _damage; }
    /* La croce in undici caratteri, es. ".X.|XXX|.X." */
    const char *map() const { return _map; }

    /* Conteggi della sessione. */
    uint16_t shots()  const { return _shots; }
    uint16_t sensorShots(uint8_t ch) const;
    void reset();

    /* Stampa la croce con la percentuale di colpi vista da ogni sensore:
       e' la lettura che serve in taratura. */
    void report(Print &out) const;

private:
    XOpenIRMulti _multi;

    uint8_t  _mask, _team, _player, _damage;
    char     _map[12];
    bool     _hit;

    uint8_t  _evMask;
    uint32_t _evMs;

    uint8_t  _ledMask;
    uint32_t _ledUntil;

    uint16_t _shots;
    uint16_t _chShots[XOPEN_CROSS_CHANNELS];

    void writeLeds(uint8_t mask);
    void buildMap();
};

#endif /* XOPENIR_H */

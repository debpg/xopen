/*
 * XOpenTarget - bersaglio X-TAG per la taratura delle repliche.
 *
 * Legge i colpi infrarossi con un TSOP4840, accende un LED e manda un
 * impulso su tre uscite digitali. Sul monitor seriale esce una riga per
 * colpo: serve per misurare portata e allineamento di una replica.
 *
 * Collegamenti (Arduino Nano):
 *
 *     D2 <- uscita del TSOP4840 (deve stare qui)
 *     D5 -> LED di colpo, con resistenza da 330 ohm
 *     D6 -> uscita 1   \
 *     D7 -> uscita 2    >  vanno alte ad ogni colpo
 *     D8 -> uscita 3   /
 *     D4 <- pulsante verso massa (azzera il conteggio)
 *
 * Schema e lista componenti: hardware/target.md
 */

#include <XOpenIR.h>

// ---- da qui si cambia quello che serve -----------------------------

const uint8_t TSOP     = 2;
const uint8_t LED_COLPO = 5;
const uint8_t PULSANTE = 4;

const uint8_t USCITE[] = { 6, 7, 8 };
const uint8_t N_USCITE = sizeof(USCITE);

// Per quanto restano accesi il LED e le uscite dopo un colpo.
const uint16_t IMPULSO_MS = 150;

// --------------------------------------------------------------------

XOpenIRRecv ir(TSOP);

uint16_t colpi = 0;
uint32_t msUltimoColpo = 0;
uint32_t spegniA = 0;

void accendi(bool acceso)
{
  digitalWrite(LED_COLPO, acceso);
  for (uint8_t i = 0; i < N_USCITE; i++) digitalWrite(USCITE[i], acceso);
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_COLPO, OUTPUT);
  pinMode(PULSANTE, INPUT_PULLUP);
  for (uint8_t i = 0; i < N_USCITE; i++) pinMode(USCITE[i], OUTPUT);
  accendi(false);

  ir.begin();

  Serial.println(F("XOpenTarget - bersaglio di taratura X-TAG"));
  Serial.println(F("colpo;ms;pausa_ms;team;giocatore;danno"));
}

void loop()
{
  XOpenFrame f;

  // read() torna false se il frame e' arrivato rovinato: si ignora.
  if (ir.available() && ir.read(f)) {
    uint32_t ora = millis();

    if (f.isSystem) {
      Serial.println(F("comando di sistema"));

    } else if (f.damage == 0) {
      // La replica ne manda uno all'accensione per provare l'emettitore.
      Serial.println(F("autotest della replica"));

    } else {
      colpi++;
      Serial.print(colpi);        Serial.print(';');
      Serial.print(ora);          Serial.print(';');
      Serial.print(msUltimoColpo ? (ora - msUltimoColpo) : 0); Serial.print(';');
      Serial.print(f.team);       Serial.print(';');
      Serial.print(f.player);     Serial.print(';');
      Serial.println(f.damage);

      msUltimoColpo = ora;
      accendi(true);
      spegniA = ora + IMPULSO_MS;
    }
  }

  if (spegniA && millis() >= spegniA) {
    accendi(false);
    spegniA = 0;
  }

  if (digitalRead(PULSANTE) == LOW) {
    colpi = 0;
    msUltimoColpo = 0;
    Serial.println(F("azzerato"));
    delay(300);
  }
}

/*
 * XOpenTool - telecomando IR X-TAG: respawn, munizioni, cura.
 *
 * Tre pulsanti e un LED infrarosso. Si punta il visore di un giocatore e
 * si preme. Tenendo premuto il comando si ripete.
 *
 * Non c'e' nessun comando che toglie vita: e' un dispositivo di supporto,
 * non un'arma.
 *
 * Collegamenti (Arduino Nano):
 *
 *     D3 -> LED infrarosso con una resistenza da 390 ohm
 *     D4 <- pulsante RESPAWN     \
 *     D5 <- pulsante MUNIZIONI    >  verso massa
 *     D6 <- pulsante CURA        /
 *
 * Il LED infrarosso deve stare su D3: la portante a 40 kHz esce da li'.
 *
 * Con 390 ohm passano circa 9 mA e il LED sta attaccato al pin senza
 * altro. Per piu' portata si pilota lo stesso pin con un MOSFET e si
 * scende a 33 ohm: lo sketch e' identico, cambia solo il cablaggio.
 *
 * Schema e lista componenti: hardware/tool.md
 */

#include <XOpenIR.h>

// ---- da qui si cambia quello che serve -----------------------------

const uint8_t PULSANTE_RESPAWN   = 4;
const uint8_t PULSANTE_MUNIZIONI = 5;
const uint8_t PULSANTE_CURA      = 6;

const uint8_t PUNTI_VITA = 100;   // quanto cura il pulsante CURA

// Pausa fra un invio e il successivo tenendo premuto il pulsante.
const uint16_t PAUSA_MS = 700;

// --------------------------------------------------------------------

XOpenIRSend ir;

uint32_t ultimoRespawn   = 0;
uint32_t ultimoMunizioni = 0;
uint32_t ultimaCura      = 0;

// true se il pulsante e' premuto ed e' passata abbastanza pausa
// dall'ultimo invio. Fa anche da antirimbalzo.
bool premuto(uint8_t pin, uint32_t &ultimoInvio)
{
  if (digitalRead(pin) == HIGH) return false;
  if (millis() - ultimoInvio < PAUSA_MS) return false;

  ultimoInvio = millis();
  return true;
}

void setup()
{
  Serial.begin(115200);

  pinMode(PULSANTE_RESPAWN, INPUT_PULLUP);
  pinMode(PULSANTE_MUNIZIONI, INPUT_PULLUP);
  pinMode(PULSANTE_CURA, INPUT_PULLUP);

  ir.begin();

  Serial.println(F("XOpenTool - telecomando X-TAG"));
  Serial.println(F("Dalla seriale: r respawn, a munizioni, c cura."));
  Serial.println(F("Se il visore ha il blocco con password, li ignora tutti."));
}

void loop()
{
  if (premuto(PULSANTE_RESPAWN, ultimoRespawn)) {
    ir.respawn();
    Serial.println(F("RESPAWN"));
  }

  if (premuto(PULSANTE_MUNIZIONI, ultimoMunizioni)) {
    ir.fullAmmo();
    Serial.println(F("MUNIZIONI"));
  }

  if (premuto(PULSANTE_CURA, ultimaCura)) {
    ir.heal(PUNTI_VITA);
    Serial.println(F("CURA"));
  }

  // Stessi comandi dalla seriale, comodi per provare senza i pulsanti.
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') { ir.respawn();        Serial.println(F("RESPAWN"));   }
    if (c == 'a') { ir.fullAmmo();       Serial.println(F("MUNIZIONI")); }
    if (c == 'c') { ir.heal(PUNTI_VITA); Serial.println(F("CURA"));      }
  }
}

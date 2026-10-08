/*
 * XOpenCross - croce di taratura X-TAG.
 *
 * Cinque TSOP4840 disposti a croce, ognuno con il suo LED. Ad ogni colpo
 * si accendono i LED dei sensori che il fascio ha davvero coperto: si
 * vede a occhio, senza guardare il computer, se la replica e' centrata e
 * quanto e' largo il cono a quella distanza.
 *
 * Collegamenti (Arduino Nano):
 *
 *     A0 <- TSOP centro       D8  -> LED centro
 *     A1 <- TSOP alto         D9  -> LED alto
 *     A2 <- TSOP basso        D10 -> LED basso
 *     A3 <- TSOP sinistra     D11 -> LED sinistra
 *     A4 <- TSOP destra       D12 -> LED destra
 *
 *     D2 <- pulsante verso massa (stampa il riepilogo)
 *
 * I sensori vanno su A0..A4 e i LED su D8..D12: la libreria li cerca li'.
 *
 * Schema, distanze e uso: hardware/cross.md
 */

#include <XOpenIR.h>

const uint8_t PULSANTE = 2;

XOpenIRCross croce;

void setup()
{
  Serial.begin(115200);
  pinMode(PULSANTE, INPUT_PULLUP);

  croce.begin();

  Serial.println(F("XOpenCross - croce di taratura X-TAG"));
  Serial.println(F("Premi il pulsante per il riepilogo."));
}

void loop()
{
  croce.loop();

  // Un colpo e' arrivato ed e' stato raccolto da tutti i sensori che
  // l'hanno visto.
  if (croce.hit()) {
    Serial.print(croce.shots());
    Serial.print(F("  "));
    Serial.print(croce.map());          // es.  .X.|XXX|.X.
    Serial.print(F("  sensori "));
    Serial.print(croce.sensors());
    Serial.print(F("/5  danno "));
    Serial.println(croce.damage());
  }

  // Pulsante premuto: la croce con le percentuali.
  if (digitalRead(PULSANTE) == LOW) {
    croce.report(Serial);
    delay(500);                         // per non ristamparlo in continuazione
  }
}

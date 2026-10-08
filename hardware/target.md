# XOpen Target — schema e componenti

Bersaglio da banco per tarare l'emettitore IR di una replica X-TAG.

## Lista componenti

| Q.tà | Componente | Note |
|---:|---|---|
| 1 | Arduino Nano (ATmega328P, 16 MHz) | va bene anche Uno o Pro Mini 5 V |
| 1 | TSOP4840 | ricevitore IR 40 kHz, 3 pin |
| 1 | Resistenza 100 Ω | in serie all'alimentazione del TSOP |
| 1 | Condensatore 4,7 µF | fra Vs del TSOP e GND |
| 1 | LED rosso 5 mm | segnalazione colpo |
| 1 | Resistenza 330 Ω | in serie al LED |
| 1 | Pulsante NA | azzeramento contatori |
| 3 | (opzionale) MOSFET logic-level o modulo relay | uscite ausiliarie |

Il filtro `100 Ω + 4,7 µF` sull'alimentazione del TSOP è quello raccomandato
da Vishay: senza, il ricevitore diventa sensibile al rumore dell'alimentazione
e comincia a perdere colpi.

## Piedinatura del TSOP4840

Guardando la **parte bombata** (la lente) rivolta verso di te, con i piedini
in basso:

```
   ___
  /   \        1  OUT   -> Arduino D2
 |     |       2  GND   -> GND
 |_____|       3  Vs    -> +5 V tramite 100 ohm
  | | |
  1 2 3
```

L'uscita ha già un pull-up interno: va collegata direttamente a D2, senza
resistenze aggiuntive.

## Collegamenti

```
  +5V ──┬── 100R ──┬── Vs (TSOP pin 3)
        │          │
        │        4u7 ─── GND
        │
        └────────────────────────────── (resto del circuito)

  TSOP OUT (pin 1) ─────────────────── D2      (interrupt esterno INT0)
  TSOP GND (pin 2) ─────────────────── GND

  D5  ── 330R ── LED rosso ── GND               LED colpo
  D13 ────────────────────────                  LED di vita (già a bordo)

  D4  ── pulsante ── GND                        azzeramento (pull-up interno)

  D6 ─┐
  D7 ─┼── uscite ausiliarie, impulso 150 ms ad ogni colpo valido
  D8 ─┘
```

**D2 non è negoziabile**: lo sketch decodifica il frame dentro l'interrupt
esterno, e sul Nano gli unici pin che ne hanno uno sono D2 e D3.

## Uscite ausiliarie

D6, D7 e D8 vanno alte per 150 ms ad ogni colpo valido. Sono pensate per
pilotare — sempre tramite un MOSFET o un modulo relay, mai direttamente —
un solenoide che ribalta il bersaglio, un buzzer, una seconda luce, o un
ingresso di un contatore esterno.

Se ti serve un altro pin, o un impulso più lungo, sono le prime righe
dello sketch: `USCITE[]` e `IMPULSO_MS`.

Un microcontrollore non alimenta un solenoide. Ogni carico induttivo vuole
il suo diodo di ricircolo e la sua alimentazione separata.

## Uso in taratura

Collega la seriale a 115200 baud. Ogni colpo valido produce una riga:

```
colpo;ms;pausa_ms;team;giocatore;danno
1;12043;0;1;5;25
2;12298;255;1;5;25
```

- **pausa_ms** è l'intervallo dal colpo precedente: misura la cadenza reale
  della replica. Attenzione, l'emettitore X-TAG genera il frame con le
  interruzioni disabilitate, quindi il `millis()` della replica perde
  ~24–32 ms per colpo: la cadenza misurata qui è quella vera, quella
  dichiarata dalla replica no.
- `autotest della replica` compare all'accensione della replica:
  è il frame di autotest che il firmware emette apposta. Se non lo vedi,
  il problema è a monte dell'ottica.

Per una prova di portata: 20 colpi a distanza fissa, conta le righe valide.
Sotto il 90% conviene rivedere allineamento della lente o corrente del LED.

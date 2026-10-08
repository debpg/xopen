# XOpen Tool — schema e componenti

Telecomando infrarosso: respawn, munizioni, cura.

Due versioni, stesso sketch: cambia solo cosa c'è attaccato a D3.

## Versione semplice — LED IR direttamente sul pin

Nessun MOSFET, nessuna saldatura difficile: il LED infrarosso va sul pin con
la sua resistenza e basta.

| Q.tà | Componente | Note |
|---:|---|---|
| 1 | Arduino Nano (ATmega328P, 16 MHz) | va bene anche Uno o Pro Mini 5 V |
| 1 | LED IR 940 nm | TSAL6200, TSAL6400 o SFH4545 |
| 1 | Resistenza 390 Ω | limitazione di corrente |
| 3 | Pulsanti NA | respawn, munizioni, cura |
| 1 | Portabatterie 4×AA o powerbank USB | |

```
  D3 ──── 390R ──── LED IR (anodo -> catodo) ──── GND
```

Con 390 Ω passano circa 9 mA: il pin di un ATmega328P ne dà 20 in sicurezza,
quindi si sta larghi. **Non scendere sotto i 330 Ω** per guadagnare portata —
oltre i 20 mA si stressa il pin, e un pin bruciato non si sostituisce.

La portata è circa un terzo di quella della versione col MOSFET: qualche
metro, non decine. Per quello che deve fare va benissimo — a un punto di
respawn o a un posto medico al giocatore ci si va vicino comunque.

## Versione con MOSFET — più portata

Stesso sketch, un transistor in più. Serve se vuoi rimettere in gioco qualcuno
da lontano senza avvicinarti.

| Q.tà | Componente | Note |
|---:|---|---|
| 1 | MOSFET logic-level N | 2N7000, BS170 o IRLML2502 |
| 1 | Resistenza 33 Ω 1/2 W | al posto della 390 Ω |
| 1 | Resistenza 100 kΩ | pull-down fra gate e source |

```
   +5V ──── 33R ──── LED IR (anodo -> catodo) ──┐
                                                │
                                             D (drain)
   D3 ──┬──────────────── G (gate)   MOSFET 2N7000
        │                         S (source)
      100k                           │
        │                           GND
       GND
```

Con 33 Ω la corrente di picco è circa (5 V − 1,4 V) / 33 Ω ≈ 110 mA. È sopra
i 100 mA di continua del TSAL6200, ma la portante ha duty 1/3 e il frame dura
una trentina di millisecondi ogni pressione: il LED sta larghissimo. Non
togliere la resistenza — si brucia il LED e basta.

Per ancora più portata si mettono **due o tre LED in serie** sullo stesso
ramo, alzando la tensione di alimentazione del ramo (9 V per tre LED) e
ricalcolando la resistenza. Il MOSFET commuta indifferentemente.

## Collegamenti

```
  D3  ──  LED IR, diretto o tramite MOSFET

  D4  ── pulsante ── GND        RESPAWN
  D5  ── pulsante ── GND        MUNIZIONI
  D6  ── pulsante ── GND        CURA

  D13 ── LED a bordo            conferma invio
```

I pulsanti usano il pull-up interno: nessuna resistenza esterna.

**D3 non è negoziabile**: la portante a 40 kHz nasce dal Timer2 in PWM
phase-correct con TOP su OCR2A, e l'unica uscita disponibile in quella
modalità è OC2B, cioè D3 sul Nano. Come effetto collaterale il Timer2 è
occupato: niente `tone()`, niente `analogWrite()` su D3 e D11.

## Comandi

| Pulsante | Effetto sul visore |
|---|---|
| RESPAWN | rimette in gioco il giocatore |
| MUNIZIONI | munizioni al massimo |
| CURA | aggiunge punti vita (100 di default) |

Gli stessi tre comandi si danno da seriale a 115200 baud: `r`, `a`, `c`.
Tutti si ripetono tenendo premuto il pulsante.

Respawn e munizioni vengono mandati tre volte di fila: il visore conta i
frame identici e agisce solo al terzo. Lo fa già la libreria, non è una cosa
di cui preoccuparsi.

## Niente comandi che fanno danno

Manda solo comandi di supporto: non spara colpi e non manda kill né
stordimento, e la libreria non contiene il codice per generarli.

Nelle partite con blocco con password il visore scarta comunque respawn,
munizioni e cura: in torneo questo attrezzo è inerte.

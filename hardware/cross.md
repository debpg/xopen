# XOpen Cross — schema e componenti

Croce di taratura a cinque sensori: dice dove passa il fascio, non solo
se passa.

## Lista componenti

| Q.tà | Componente | Note |
|---:|---|---|
| 1 | Arduino Nano (ATmega328P, 16 MHz) | |
| 5 | Ricevitore infrarosso TSOP4840 | deve essere la versione a 40 kHz |
| 5 | Resistenza 100 Ω | una per sensore |
| 5 | Condensatore 4,7 µF | uno per sensore |
| 1 | LED verde 5 mm | centro |
| 4 | LED rossi 5 mm | braccia |
| 5 | Resistenza 330 Ω | una per LED |
| 1 | Pulsante NA | riepilogo |
| 1 | Basetta millefori o supporto stampato in 3D | |

Il filtro `100 Ω + 4,7 µF` va messo **su ogni sensore**, non uno solo per
tutta la croce. Cinque TSOP sullo stesso ramo di alimentazione si
disturbano a vicenda e il sintomo è una pioggia di colpi persi.

## Collegamenti

```
  +5V ──┬── 100R ──┬── Vs  TSOP centro  ── OUT ── A0
        │        4u7 ── GND
        ├── 100R ──┬── Vs  TSOP alto    ── OUT ── A1
        │        4u7 ── GND
        ├── 100R ──┬── Vs  TSOP basso   ── OUT ── A2
        │        4u7 ── GND
        ├── 100R ──┬── Vs  TSOP sinistra── OUT ── A3
        │        4u7 ── GND
        └── 100R ──┬── Vs  TSOP destra  ── OUT ── A4
                 4u7 ── GND

  D8  ── 330R ── LED verde ── GND      centro
  D9  ── 330R ── LED rosso ── GND      alto
  D10 ── 330R ── LED rosso ── GND      basso
  D11 ── 330R ── LED rosso ── GND      sinistra
  D12 ── 330R ── LED rosso ── GND      destra

  D2  ── pulsante ── GND               riepilogo
```

**A0…A4 e D8…D12 non sono negoziabili.** I sensori stanno tutti su PORTC
e i LED tutti su PORTB perché il campionamento legge una porta intera in
una sola istruzione: cinque `digitalRead()` dentro un'interruzione che
gira ogni 50 µs non ci starebbero.

## Come funziona dentro

Un ATmega328P ha **due** interrupt esterni, non cinque. Per leggere
cinque sensori indipendenti si fa come nel firmware X-TAG: un timer
campiona tutti gli ingressi a intervallo fisso e ogni canale ha la sua
macchina a stati. Qui il Timer2 interrompe ogni 50 µs — l'impulso più
corto del protocollo dura 550 µs, quindi vengono presi una decina di
campionamenti per bit, che bastano e avanzano.

Una differenza voluta rispetto al firmware originale: là, appena un
sensore dell'headset completa un frame, gli altri smettono di essere
campionati, perché un colpo solo visto da tre sensori non deve contare
tre volte il danno. Qui il danno non lo conta nessuno e sapere **quali**
sensori sono stati coperti è tutto il punto della misura, quindi quella
de-duplica non c'è: i cinque canali corrono davvero in parallelo.

Il Timer2 è occupato dal campionamento: niente `tone()` e niente
`analogWrite()` su D3 e D11.

## Quanto distanti le braccia

Dipende dalla distanza a cui vuoi provare, e la risposta utile è una
sola: **il braccio va messo circa a metà del diametro del cono a quella
distanza.** Più stretto e si accendono sempre tutti e cinque; più largo e
si accende sempre e solo il centro. Nel punto giusto la croce diventa
sensibile al puntamento, che è quello che serve.

I coni dichiarati X-Tag danno il fattore di crescita:

| Configurazione | Cono | Crescita | Braccia per una prova a 10 m |
|---|---|---|---:|
| Assalto | 70 cm a 100 m | 0,7 cm/m | **3,5 cm** |
| SMG | 90 cm a 70 m | 1,3 cm/m | 6,5 cm |
| Pistola | 100 cm a 30 m | 3,3 cm/m | 16 cm |
| Sniper | 20 cm a 200 m | 0,1 cm/m | 0,5 cm — provala da lontano |

Per una croce da banco tuttofare, **braccia a 4 cm dal centro**: va bene
per un kit d'assalto intorno ai dieci metri, che è la prova che si fa più
spesso. Con lo sniper la croce ha senso a cento metri e oltre, dove il
cono si allarga abbastanza da essere misurabile.

I TSOP sono larghi sette millimetri, quindi anche a 3,5 cm di interasse
non si toccano.

**Niente separatori fra un sensore e l'altro.** Verrebbe la tentazione di
schermarli per "isolarli", ma sarebbe un errore: la croce misura se il
fascio illumina quel punto, non da che parte arriva. Un setto riduce il
campo visivo del sensore e falsa la misura.

## Uso

Monitor seriale a 115200 baud. Ogni colpo produce una riga:

```
12  .X.|XXX|.X.  sensori 5/5  danno 25
13  ...|.XX|...  sensori 2/5  danno 25
```

La mappa è la croce vista da davanti: `X` sensore coperto, `.` sensore al
buio. La riga 13 qui sopra dice che il fascio ha preso centro e destra ma
non sinistra, alto e basso: la replica punta a destra e il cono è stretto.

Premendo il pulsante esce il riepilogo con la percentuale di colpi vista
da ogni sensore:

```
colpi: 20
        95%
  40%  100%   45%
        90%
```

Questa è la lettura che serve. Centro al 100% e braccia tutte simili
significa puntamento centrato. Sinistra e destra molto diverse fra loro
vogliono dire che l'emettitore è storto sull'asse orizzontale; alto e
basso diversi, che è storto in elevazione. Braccia tutte basse con il
centro pieno: il cono è più stretto delle braccia, avvicina la croce o
stringi le braccia.

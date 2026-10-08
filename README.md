# XOpen

Progetti aperti per costruirsi accessori compatibili con il sistema lasertag
**X-TAG**, usando un Arduino Nano, un LED infrarosso e un TSOP4840.

Tre dispositivi, una manciata di righe di codice ciascuno, pochi euro di
componenti. Gli sketch sono volutamente corti: la parte complicata sta nella
libreria, quella che non serve aprire.

| Progetto | Cosa fa |
|---|---|
| [**XOpen Target**](firmware/XOpenTarget) | Bersaglio da banco: legge i colpi IR, accende un LED di segnalazione, impulsa alcune uscite digitali e stampa su seriale una riga per colpo. Serve a tarare l'emettitore di una replica. |
| [**XOpen Cross**](firmware/XOpenCross) | Croce di taratura: cinque TSOP e cinque LED disposti a croce. Dice quali sensori il fascio ha coperto, quindi dove punta la replica e quanto è largo il cono. |
| [**XOpen Tool**](firmware/XOpenTool) | Telecomando IR di supporto: respawn, munizioni, cura. Tre pulsanti e un LED infrarosso attaccato al pin, senza altro. |

Entrambi usano [`XOpenIR`](firmware/libraries/XOpenIR), una libreria di due file
che incapsula il frame infrarosso X-TAG (portante 40 kHz, 17 bit, parità pari).

## Schemi

- [Schema del bersaglio](hardware/target.md)
- [Schema della croce](hardware/cross.md)
- [Schema del telecomando](hardware/tool.md)

## Installazione

1. Copia `firmware/libraries/XOpenIR` dentro la cartella `libraries` del tuo
   sketchbook Arduino (di solito `Documenti/Arduino/libraries/`).
2. Apri lo sketch del progetto che ti interessa, in `firmware/`.
3. Seleziona **Arduino Nano** con il processore giusto (ATmega328P, o
   "ATmega328P (Old Bootloader)" per i cloni) e carica.

Nessuna dipendenza esterna. Non usare IRremote: i frame X-TAG non seguono
nessuno dei protocolli standard che quella libreria conosce.

## Attenzione

Il Timer2 è occupato nel Tool (portante a 40 kHz) e nel Cross
(campionamento dei sensori): in entrambi niente `tone()` e niente
`analogWrite()` su D3 e D11. Nel Target è occupato l'interrupt esterno di D2.

Progetti amatoriali, non prodotti X-TAG: nessuna garanzia. Quello che
colleghi ai pin digitali è responsabilità tua, e un solenoide attaccato
direttamente a un pin di Arduino non è mai una buona idea.

Il telecomando manda solo comandi di supporto: la libreria non sa generare
colpi e non espone i comandi che tolgono vita.

Licenza MIT, vedi [LICENSE](LICENSE).

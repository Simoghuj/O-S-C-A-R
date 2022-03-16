# Nápady a poznamky
- pro řízaní motorů s epoužije asi aplikace MIT 2 z důvodu možností jako je například akcelerometer (tohleto se musí vyzkoušet)
- pro pohyb ruky se bude vypočítávat __rychlost__ pro otáčení konkrétních motorů která se následně pošle do esp _čas bude stejný_ pro zajištění plynulého pohybu
  - popřípadě kroky motorů, to se musí jeětě pořádně vyzkoušet 

## step a dir 
- na krokovce 3 jsou připojeny piny STEP a DIR, kterými je možnost ovládat směr a jednotlivé kroky.
  - STEP - IO05
  - DIR - IO04

## vepisování rychlosti a přepočet 
- rychlost se vepisuje do 0x22 jako hodnota napětí na motor
- čte se krokování z INDEX (RW GCONF register )
  - nastaví co zobrazuje INDEX pin 
- 
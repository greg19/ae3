# Zadanie 3

```bash
make
./main.e data/*.in
python analyze.py data/*.csv
```

Program w C++ przyjmuje plik w następującym formacie:

```
<liczba iteracji ficticious play>
<BA> <BD> <liczba pól bitew>
wartości kolejnych pól bitew
<wielkość supportu początkowej strategii atakującego>
kolejne strategie czyste atakującego
<wielkość supportu początkowej strategii broniącego>
kolejne strategie czyste broniącego
```

Strategie wczytywane są jako ciągi binarne. Jeżeli jako wielkość support będzie
podane 0, to zostanie użyta strategia jednorodna.

Generuje 3 pliki `csv`. Plik z rozszerzeniem `.csv` opisuje kolejne iteracje
algorytmu, a pliki `.attacker` i `.defender` końcowe strategie graczy.


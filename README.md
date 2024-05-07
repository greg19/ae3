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
```

I generuje 3 pliki `csv`. Plik z rozszerzeniem `.csv` opisuje kolejne iteracje
algorytmu, a pliki `.attacker` i `.defender` końcowe strategie graczy.


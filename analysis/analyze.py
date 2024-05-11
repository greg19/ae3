import matplotlib.pyplot as plt
import pandas as pd
import sys

for filename in sys.argv[1:]:
    df = pd.read_csv(filename)

    plt.clf()
    plt.plot(df['epsilon'], linewidth=0.5)
    plt.yscale("log")
    plt.savefig(filename + ".epsilon.png", dpi=300)

    plt.clf()
    plt.plot(df['payoff'], linewidth=0.5)
    plt.savefig(filename + ".payoff.png", dpi=300)


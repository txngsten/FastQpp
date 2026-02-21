import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

df = pd.read_csv("benchmark_results/baseline_drain.csv")
df_pivot = df.pivot(index="Threads", columns="Queue", values="TotalOps")

fig, ax = plt.subplots()

df_pivot.plot(marker="o", ax=ax)

ax.ticklabel_format(style='plain', axis='y')
ax.yaxis.set_major_formatter(mtick.StrMethodFormatter('{x:,.0f}'))

ax.set_ylim(df["TotalOps"].min() * 0.8,
            df["TotalOps"].max() * 1.2)


ax.tick_params(axis='y', which='minor', labelsize=8)

plt.xlabel("Threads")
plt.ylabel("Ops/s per thread")
plt.title("Drain/Deque Throughput")
plt.grid(True, which="both", linestyle="--", linewidth=0.5)

plt.savefig("benchmark.png", dpi=800)
plt.show()
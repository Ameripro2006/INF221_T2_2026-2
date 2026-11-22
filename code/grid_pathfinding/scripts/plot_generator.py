# Estudiante: Tu Nombre | Rol USM: Tu Rol | Campus: Tu Campus | Tarea 2 — INF-221 2026-2
"""
Script para generar los 4 gráficos PNG de análisis experimental a partir de
data/measurements/measurements.csv y guardarlos en data/plots/.

Gráficos generados:
  1. tiempo_vs_nm.png       — Tiempo promedio (ms) vs. N·M (escala log-log).
  2. memoria_vs_nm.png      — Incremento promedio de memoria (KB) vs. N·M.
  3. brecha_delta_vs_nm.png — Brecha promedio Delta = Score_DP - Score_Greedy vs. N·M (D10 vs D1000).
  4. tasa_falla_greedy.png  — Tasa de falla de Greedy (% SIN SOLUCION) por tipo de grilla y dominio.
"""

import os
import sys

import matplotlib

matplotlib.use("Agg")  # Backend sin interfaz gráfica para ejecución headless
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import pandas as pd


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MEASUREMENTS_CSV = os.path.normpath(
    os.path.join(BASE_DIR, "..", "data", "measurements", "measurements.csv")
)
PLOTS_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", "data", "plots"))

# Paleta accesible (Okabe-Ito) para distinguir algoritmos y dominios
COLOR_BF = "#D55E00"
COLOR_DP = "#0072B2"
COLOR_GREEDY = "#009E73"
COLOR_D10 = "#0072B2"
COLOR_D1000 = "#D55E00"

TIPOS_ORDEN = ["libre", "minas", "murallas", "mixta"]
TIPOS_ETIQUETAS = ["Libre", "Minas", "Murallas", "Mixta"]
DOMINIOS_ORDEN = ["D10", "D1000"]


def configurar_estilo():
    """Configura un estilo limpio y legible para los gráficos del informe."""
    plt.rcParams.update(
        {
            "font.size": 10.5,
            "axes.titlesize": 12.5,
            "axes.titleweight": "bold",
            "axes.labelsize": 11,
            "xtick.labelsize": 9.5,
            "ytick.labelsize": 9.5,
            "legend.fontsize": 9.5,
            "figure.dpi": 120,
            "savefig.dpi": 220,
            "axes.spines.top": False,
            "axes.spines.right": False,
        }
    )


def cargar_datos(csv_path):
    """Carga measurements.csv, limpia columnas numéricas y calcula N·M."""
    if not os.path.isfile(csv_path):
        raise FileNotFoundError(f"No se encontró el archivo de mediciones: {csv_path}")

    df = pd.read_csv(csv_path)
    columnas_requeridas = {
        "instance",
        "n",
        "m",
        "tipo",
        "dominio",
        "muestra",
        "algoritmo",
        "found",
        "score",
        "vidas",
        "tiempo_ms",
        "mem_kb",
    }
    faltantes = columnas_requeridas - set(df.columns)
    if faltantes:
        raise ValueError(f"Faltan columnas en measurements.csv: {sorted(faltantes)}")

    for col in ("instance", "tipo", "dominio", "muestra", "algoritmo"):
        df[col] = df[col].astype(str).str.strip()

    for col in ("n", "m", "found", "score", "vidas", "tiempo_ms", "mem_kb"):
        df[col] = pd.to_numeric(df[col], errors="coerce")

    df["nm"] = df["n"] * df["m"]
    return df


def calcular_brecha_delta(df):
    """Une filas de DP y Greedy (ambos con found == 1) por instance y calcula Delta."""
    dp = df[(df["algoritmo"] == "dp") & (df["found"] == 1)][
        ["instance", "n", "m", "nm", "tipo", "dominio", "muestra", "score"]
    ].rename(columns={"score": "score_dp"})

    greedy = df[(df["algoritmo"] == "greedy") & (df["found"] == 1)][
        ["instance", "score"]
    ].rename(columns={"score": "score_greedy"})

    merged = pd.merge(dp, greedy, on="instance", how="inner")
    merged = merged.dropna(subset=["score_dp", "score_greedy", "nm"])
    merged["delta"] = merged["score_dp"] - merged["score_greedy"]
    return merged


def graficar_tiempo_vs_nm(df, output_path):
    """Gráfico 1: Tiempo promedio (ms) vs N·M en escala log-log."""
    fig, ax = plt.subplots(figsize=(7.6, 4.8))

    series_cfg = [
        ("bruteforce", "Fuerza Bruta (N + M ≤ 32)", COLOR_BF, "s", "-", 2.1, 6.0),
        ("dp", "Programación Dinámica", COLOR_DP, "o", "-", 2.1, 6.0),
        ("greedy", "Greedy", COLOR_GREEDY, "^", "-", 2.1, 6.2),
    ]

    grouped = (
        df.dropna(subset=["nm", "tiempo_ms"])
        .groupby(["algoritmo", "nm"], as_index=False)["tiempo_ms"]
        .mean()
        .sort_values("nm")
    )

    plotted_x = []
    plotted_y = []
    has_bf = False

    for algo, label, color, marker, linestyle, lw, ms in series_cfg:
        sub = grouped[grouped["algoritmo"] == algo]
        if sub.empty:
            continue
        ax.plot(
            sub["nm"],
            sub["tiempo_ms"],
            label=label,
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=lw,
            markersize=ms,
        )
        plotted_x.extend(sub["nm"].tolist())
        plotted_y.extend(sub["tiempo_ms"].tolist())
        if algo == "bruteforce":
            has_bf = True

    if plotted_x and all(x > 0 for x in plotted_x):
        ax.set_xscale("log")

    if plotted_y and all(y > 0 for y in plotted_y):
        ax.set_yscale("log")
        ylabel = "Tiempo promedio (ms, escala logarítmica)"
    else:
        ax.set_yscale("symlog", linthresh=1e-4)
        ylabel = "Tiempo promedio (ms, escala symlog)"

    if has_bf and plotted_x and max(plotted_x) > 256:
        ax.axvline(256, color="#666666", linestyle=":", linewidth=1.2, alpha=0.85)
        y_low, y_high = ax.get_ylim()
        # Ubicamos la nota junto al límite N·M = 256 en zona despejada
        if ax.get_yscale() == "log" and y_low > 0:
            y_text = (y_low**0.20) * (y_high**0.80)
        else:
            y_text = y_low + 0.75 * (y_high - y_low)
        ax.annotate(
            "Límite Fuerza Bruta\n(N + M = 32, N·M = 256)",
            xy=(256, y_text),
            xytext=(14, 0),
            textcoords="offset points",
            fontsize=8.8,
            color="#444444",
            va="center",
            bbox=dict(
                boxstyle="round,pad=0.3",
                facecolor="white",
                edgecolor="#CCCCCC",
                alpha=0.9,
            ),
        )

    ax.set_title("Tiempo promedio de ejecución vs. tamaño de grilla (N · M)")
    ax.set_xlabel("Tamaño de grilla N · M (celdas, escala logarítmica)")
    ax.set_ylabel(ylabel)
    ax.grid(True, which="major", linestyle="--", linewidth=0.6, alpha=0.45)
    ax.grid(True, which="minor", linestyle=":", linewidth=0.4, alpha=0.25)

    if ax.get_legend_handles_labels()[0]:
        ax.legend(loc="lower right", frameon=True, framealpha=0.95)

    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def graficar_memoria_vs_nm(df, output_path):
    """Gráfico 2: Incremento promedio de memoria (mem_kb) vs N·M."""
    fig, ax = plt.subplots(figsize=(7.6, 4.8))

    grouped = (
        df.dropna(subset=["nm", "mem_kb"])
        .groupby(["algoritmo", "nm"], as_index=False)["mem_kb"]
        .mean()
        .sort_values("nm")
    )

    series_cfg = [
        (
            "dp",
            "Programación Dinámica",
            dict(
                color=COLOR_DP,
                marker="o",
                linestyle="-",
                linewidth=2.2,
                markersize=6.2,
                zorder=4,
            ),
        ),
        (
            "greedy",
            "Greedy",
            dict(
                color=COLOR_GREEDY,
                marker="^",
                linestyle="--",
                linewidth=1.8,
                markersize=6.2,
                zorder=3,
            ),
        ),
        (
            "bruteforce",
            "Fuerza Bruta (N + M ≤ 32)",
            dict(
                color=COLOR_BF,
                marker="s",
                linestyle=":",
                linewidth=2.0,
                markersize=7.6,
                markerfacecolor="none",
                markeredgewidth=1.8,
                zorder=5,
            ),
        ),
    ]

    plotted_x = []
    plotted_y = []

    for algo, label, style in series_cfg:
        sub = grouped[grouped["algoritmo"] == algo]
        if sub.empty:
            continue
        ax.plot(sub["nm"], sub["mem_kb"], label=label, **style)
        plotted_x.extend(sub["nm"].tolist())
        plotted_y.extend(sub["mem_kb"].tolist())

    if plotted_x and all(x > 0 for x in plotted_x):
        ax.set_xscale("log")

    if plotted_y:
        y_max = max(plotted_y)
        y_min = min(plotted_y)
        margen_inf = -0.05 * y_max if y_max > 0 else -1.0
        margen_sup = y_max * 1.12 if y_max > 0 else 10.0
        ax.set_ylim(min(y_min, margen_inf), margen_sup)

    ax.yaxis.set_major_formatter(
        FuncFormatter(lambda val, _: f"{int(round(val)):,}".replace(",", " "))
    )

    ax.set_title("Incremento de memoria (ru_maxrss) vs. tamaño de grilla (N · M)")
    ax.set_xlabel("Tamaño de grilla N · M (celdas, escala logarítmica)")
    ax.set_ylabel("Incremento promedio de memoria (KB)")
    ax.grid(True, which="major", linestyle="--", linewidth=0.6, alpha=0.45)

    if ax.get_legend_handles_labels()[0]:
        ax.legend(loc="upper left", frameon=True, framealpha=0.95)

    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def graficar_brecha_delta_vs_nm(merged, output_path):
    """Gráfico 3: Brecha promedio Delta = Score_DP - Score_Greedy vs N·M (D10 y D1000)."""
    fig, ax = plt.subplots(figsize=(7.6, 4.8))

    if merged.empty:
        ax.text(
            0.5,
            0.5,
            "Sin pares válidos (DP y Greedy con solución)",
            ha="center",
            va="center",
            transform=ax.transAxes,
        )
        ax.set_title(r"Brecha promedio $\Delta = \mathrm{Score}_{\mathrm{DP}} - \mathrm{Score}_{\mathrm{Greedy}}$ vs. $N \cdot M$")
        fig.tight_layout()
        fig.savefig(output_path, bbox_inches="tight")
        plt.close(fig)
        return

    grouped = (
        merged.groupby(["dominio", "nm"], as_index=False)["delta"]
        .mean()
        .sort_values("nm")
    )

    dominios_cfg = [
        ("D1000", "Dominio D1000 ([-1000, 1000], P = 1000)", COLOR_D1000, "s"),
        ("D10", "Dominio D10 ([-10, 10], P = 10)", COLOR_D10, "o"),
    ]

    plotted_x = []
    plotted_y = []

    for dom, label, color, marker in dominios_cfg:
        sub = grouped[grouped["dominio"] == dom]
        if sub.empty:
            continue
        ax.plot(
            sub["nm"],
            sub["delta"],
            label=label,
            color=color,
            marker=marker,
            linestyle="-",
            linewidth=2.1,
            markersize=6.0,
        )
        plotted_x.extend(sub["nm"].tolist())
        plotted_y.extend(sub["delta"].tolist())

    if plotted_x and all(x > 0 for x in plotted_x):
        ax.set_xscale("log")

    if plotted_y and all(y > 0 for y in plotted_y):
        ax.set_yscale("log")
        ylabel = r"Brecha promedio $\Delta$ (escala logarítmica)"
        if 1.0 <= min(plotted_y) < 10.0:
            ax.set_ylim(bottom=1.0)
    else:
        ax.set_yscale("symlog", linthresh=1.0)
        ylabel = r"Brecha promedio $\Delta$ (escala symlog)"

    ax.set_title(
        r"Brecha promedio $\Delta = \mathrm{Score}_{\mathrm{DP}} - \mathrm{Score}_{\mathrm{Greedy}}$ vs. $N \cdot M$"
    )
    ax.set_xlabel("Tamaño de grilla N · M (celdas, escala logarítmica)")
    ax.set_ylabel(ylabel)
    ax.grid(True, which="major", linestyle="--", linewidth=0.6, alpha=0.45)
    ax.grid(True, which="minor", linestyle=":", linewidth=0.4, alpha=0.25)

    if ax.get_legend_handles_labels()[0]:
        ax.legend(loc="upper left", frameon=True, framealpha=0.95)

    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def graficar_tasa_falla_greedy(df, output_path):
    """Gráfico 4: Tasa de falla de Greedy (% found == 0) por tipo de grilla y dominio."""
    fig, ax = plt.subplots(figsize=(7.6, 4.8))

    greedy = df[df["algoritmo"] == "greedy"].dropna(subset=["found"])

    tasas = {dom: [] for dom in DOMINIOS_ORDEN}
    for dom in DOMINIOS_ORDEN:
        for tipo in TIPOS_ORDEN:
            sub = greedy[(greedy["dominio"] == dom) & (greedy["tipo"] == tipo)]
            if sub.empty:
                tasas[dom].append(0.0)
            else:
                fail_pct = (sub["found"] == 0).mean() * 100.0
                tasas[dom].append(float(fail_pct))

    posiciones = list(range(len(TIPOS_ORDEN)))
    ancho = 0.36
    x_d10 = [p - ancho / 2 for p in posiciones]
    x_d1000 = [p + ancho / 2 for p in posiciones]

    bars_d10 = ax.bar(
        x_d10,
        tasas["D10"],
        width=ancho,
        label="Dominio D10",
        color=COLOR_D10,
        edgecolor="#222222",
        linewidth=0.7,
    )
    bars_d1000 = ax.bar(
        x_d1000,
        tasas["D1000"],
        width=ancho,
        label="Dominio D1000",
        color=COLOR_D1000,
        edgecolor="#222222",
        linewidth=0.7,
    )

    max_pct = max(tasas["D10"] + tasas["D1000"], default=0.0)
    y_top = max(45.0, min(115.0, max_pct * 1.32))
    ax.set_ylim(0, y_top)

    offset_y = y_top * 0.018
    for bars in (bars_d10, bars_d1000):
        for bar in bars:
            altura = bar.get_height()
            ax.text(
                bar.get_x() + bar.get_width() / 2.0,
                altura + offset_y,
                f"{altura:.1f}%",
                ha="center",
                va="bottom",
                fontsize=9.2,
                fontweight="bold",
                color="#222222",
            )

    ax.set_xticks(posiciones)
    ax.set_xticklabels(TIPOS_ETIQUETAS)
    ax.set_title("Tasa de falla de Greedy (SIN SOLUCION) según tipo de grilla y dominio")
    ax.set_xlabel("Tipo de grilla")
    ax.set_ylabel("Tasa de falla de Greedy (%)")
    ax.grid(True, axis="y", linestyle="--", linewidth=0.6, alpha=0.45)
    ax.set_axisbelow(True)
    ax.legend(loc="upper left", frameon=True, framealpha=0.95)

    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def main():
    configurar_estilo()
    os.makedirs(PLOTS_DIR, exist_ok=True)

    df = cargar_datos(MEASUREMENTS_CSV)
    merged = calcular_brecha_delta(df)

    salidas = [
        ("tiempo_vs_nm.png", lambda p: graficar_tiempo_vs_nm(df, p)),
        ("memoria_vs_nm.png", lambda p: graficar_memoria_vs_nm(df, p)),
        ("brecha_delta_vs_nm.png", lambda p: graficar_brecha_delta_vs_nm(merged, p)),
        ("tasa_falla_greedy.png", lambda p: graficar_tasa_falla_greedy(df, p)),
    ]

    for nombre, fn in salidas:
        ruta = os.path.join(PLOTS_DIR, nombre)
        fn(ruta)
        print(f"[OK] Gráfico generado: {ruta}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        sys.exit(1)

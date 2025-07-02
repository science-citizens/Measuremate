import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# === Configuratie van drempelwaarden voor foutdetectie ===
EXPECTED = 44                # Verwachte afstand in cm
MARGIN = 3                   # Acceptabele afwijking rond de verwachte waarde
LOWER = EXPECTED - MARGIN    # Ondergrens voor geldige meting
UPPER = EXPECTED + MARGIN    # Bovengrens voor geldige meting
STABLE_STD_THRESHOLD = 0.5   # (Ongebruikt in deze versie) standaarddeviatiegrens voor stabiliteitsanalyse

# === Inlezen van de dataset ===
# CSV-bestand met tijdstempel en afstandsmetingen per sensor wordt ingelezen
df = pd.read_csv("Melle\SonarData\Threshold\SonarDataThreshold.csv")
df.columns = ['timestamp', 'sensor_1', 'sensor_2', 'sensor_3', 'sensor_4']

# Zet de timestamp-kolom om naar datetime-objecten (tijd zonder datum)
df['timestamp'] = pd.to_datetime(df['timestamp'], format='%H:%M:%S')

# === Foutdetectie op basis van drempelwaarden ===
# Voor elke sensor wordt gecontroleerd of de waarde buiten het toegestane bereik valt
error_flags = {}
for col in df.columns[1:]:
    error_flags[col] = ~df[col].between(LOWER, UPPER)  # Boolean mask: True als buiten bereik

# Voeg foutkolommen toe aan de DataFrame
for col in error_flags:
    df[f"{col}_error"] = error_flags[col]

# === Berekening van foutpercentages per sensor ===
sensors = df.columns[1:5]
error_percentages = [df[f"{col}_error"].mean() * 100 for col in sensors]  # Gemiddeld % foutieve metingen

# === Aanmaak van de plot (2 subplots: tijdserie + foutpercentages) ===
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 9), gridspec_kw={'height_ratios': [4, 1]})

colors = ['blue', 'green', 'orange', 'purple']  # Kleur per sensor

# === Plot tijdserie van afstandsmetingen met foutpunten in rood ===
for i, col in enumerate(sensors):
    ax1.plot(df['timestamp'], df[col], label=col, color=colors[i])  # Lijnplot van ruwe metingen
    error_indices = df[df[f"{col}_error"]].index  # Indexen van foutieve waarden
    ax1.scatter(df.loc[error_indices, 'timestamp'], df.loc[error_indices, col],
                color='red', s=30,
                label=f"{col} fout" if i == 0 else "")  # Rood markeren van foutmetingen

# === Plot drempellijnen (onder- en bovengrens) ===
ax1.axhline(LOWER, color='gray', linestyle='--', linewidth=1, label='Drempel ondergrens')
ax1.axhline(UPPER, color='gray', linestyle='--', linewidth=1, label='Drempel bovengrens')

# Labels en legenda
ax1.set_ylabel('Afstand (cm)')
ax1.set_title('Sonarmetingen met foutmarkering')
ax1.legend(loc='upper right')
ax1.grid(True)

# === Verbeterde tijdsas: formattering per minuut ===
ax1.xaxis.set_major_locator(mdates.MinuteLocator(interval=1))
ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
fig.autofmt_xdate(rotation=45)  # Rotatie voor leesbaarheid

# === Plot foutpercentages per sensor in een staafdiagram ===
ax2.bar(sensors, error_percentages, color=colors)
ax2.set_ylim(0, max(error_percentages) * 1.2)
ax2.set_ylabel('Foutpercentage (%)')
ax2.set_title('Foutpercentage per sensor')
ax2.grid(axis='y', linestyle='--', alpha=0.7)

# Toon foutpercentages boven de staven
for i, v in enumerate(error_percentages):
    ax2.text(i, v + 0.5, f"{v:.1f}%", ha='center', fontsize=10)

# === Lay-out en opslag ===
plt.tight_layout()
plt.savefig("Melle/SonarData/Fotos/SonarDataAvarage2.png", dpi=300, bbox_inches='tight')
plt.show()

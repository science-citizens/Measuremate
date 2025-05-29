# Script voor het verwerken van sonargegevens en foutdetectie
# Melle - Sonar Data Processor

import pandas as pd
import matplotlib.pyplot as plt

# Configuratie
EXPECTED = 45
MARGIN = 3
LOWER = EXPECTED - MARGIN
UPPER = EXPECTED + MARGIN
STABLE_STD_THRESHOLD = 0.5  # voor 'te stabiel'

# Inlezen
df = pd.read_csv("Melle\SonarData\SonarDataNoProc.csv")
df.columns = ['timestamp', 'sensor_1', 'sensor_2', 'sensor_3', 'sensor_4']
df['timestamp'] = pd.to_datetime(df['timestamp'], format='%H:%M:%S')

# Foutdetectie
error_flags = {}
for col in df.columns[1:]:
    error_flags[col] = ~df[col].between(LOWER, UPPER)

# Voeg foutkolommen toe aan DataFrame
for col in error_flags:
    df[f"{col}_error"] = error_flags[col]

# Statistieken per sensor
print("📊 Sensorstatistieken:")
total_errors = 0
total_measurements = 0

for col in df.columns[1:5]:
    sensor_vals = df[col]
    errors = df[f"{col}_error"].sum()
    total = len(sensor_vals)
    total_errors += errors
    total_measurements += total

    print(f"\n{col}:")
    print(f"  Fouten: {errors} ({(errors/total)*100:.2f}%)")
    print(f"  Gemiddelde: {sensor_vals.mean():.2f} cm")
    print(f"  Standaarddeviatie: {sensor_vals.std():.2f} cm")
    print(f"  Minimum: {sensor_vals.min():.2f} cm")
    print(f"  Maximum: {sensor_vals.max():.2f} cm")
    if sensor_vals.std() < STABLE_STD_THRESHOLD:
        print("  ⚠️  Verdacht stabiel gedrag (mogelijk defect sensor)")

# Algemene foutmarge
general_error_percentage = (total_errors / total_measurements) * 100
print("\n🧮 Algemene foutanalyse:")
print(f"  Totaal aantal metingen: {total_measurements}")
print(f"  Totaal aantal fouten: {total_errors}")
print(f"  Totale foutmarge: {general_error_percentage:.2f}%")

# Visualisatie: fouten per sensor
plt.figure(figsize=(10, 6))
for i, col in enumerate(df.columns[1:5], start=1):
    plt.plot(df['timestamp'], df[col], label=col)
    error_indices = df[df[f"{col}_error"]].index
    plt.scatter(df.loc[error_indices, 'timestamp'], df.loc[error_indices, col], color='red', label=f"{col} fout" if i == 1 else "")

plt.axhline(LOWER, color='gray', linestyle='--', linewidth=1)
plt.axhline(UPPER, color='gray', linestyle='--', linewidth=1)
plt.title("Sonarmetingen met fouten (rood)")
plt.xlabel("Tijd")
plt.ylabel("Afstand (cm)")
plt.legend()
plt.tight_layout()
plt.grid(True)
plt.show()

#pip install w1thermsensor
#Als er een foutmelding optreedt die aangeeft dat het niet geinstalleerd kan worden in
#deze omgeving, voer het script dan uit in een terminal buiten deze omgeving zoals een virtuele omgeving.

#Met de volgende stappen kun je een virtuele omgeving aanmaken en activeren:
#1. Installeer de benodigde pakketten voor virtuele omgevingen (indien nog niet gedaan)
# sudo apt install python3-venv -y

# 2. Maak een virtuele omgeving aan (mapnaam 'env' of wat je maar wilt)
# python3 -m venv env

# 3. activeer de virtuele omgeving
# source env/bin/activate

from w1thermsensor import W1ThermSensor
import time
import csv
from datetime import datetime
import os

sensor = W1ThermSensor()
# Bestandsnaam voor CSV log
csv_file = "temperature_log.csv"

# Maak bestand als het nog niet bestaat
file_exists = os.path.isfile(csv_file)
with open(csv_file, mode="a", newline="") as f:
    writer = csv.writer(f)
    if not file_exists:
        writer.writerow(["timestamp", "temperature_c"])

print("Temp log gestart")


while True:
    temp = sensor.get_temperature()  # °C
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Log naar CSV
    with open(csv_file, mode="a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, round(temp, 2)])

    # Ook printen naar console
    print(f"[{timestamp}] Temperature: {round(temp, 2)} °C")

    # Wacht 10 seconden (300 seconden is 5minuten)
    time.sleep(300)




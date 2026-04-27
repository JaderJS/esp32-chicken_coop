import os

def load_env():
    if not os.path.exists(".env"):
        print("⚠️ Arquivo .env não encontrado")
        return

    with open(".env") as f:
        for line in f:
            line = line.strip()

            # ignora vazio e comentário
            if not line or line.startswith("#"):
                continue

            if "=" in line:
                k, v = line.split("=", 1)
                os.environ[k.strip()] = v.strip()

load_env()

Import("env")

def get_env(name):
    value = os.getenv(name)
    if value is None:
        raise Exception(f"Variável {name} não definida no .env")
    return value

env.Append(
    BUILD_FLAGS=[
        f'-D WIFI_SSID=\\"{get_env("WIFI_SSID")}\\"',
        f'-D WIFI_PASS=\\"{get_env("WIFI_PASS")}\\"',
        f'-D API_KEY_WEATHER=\\"{get_env("API_KEY_WEATHER")}\\"',
        f'-D MQTT_URL=\\"{get_env("MQTT_URL")}\\"',
        f'-D MQTT_USER=\\"{get_env("MQTT_USER")}\\"',
        f'-D MQTT_PASS=\\"{get_env("MQTT_PASS")}\\"',
    ]
)
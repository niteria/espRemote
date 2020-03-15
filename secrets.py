Import("projenv")
import subprocess

def lookup_secret(key1, key2):
    return subprocess.check_output("secret-tool lookup " +
            key1 + " " + key2, shell=True).decode('utf-8')


# append extra flags to only project build environment
projenv.Append(CPPDEFINES=[
    ("WIFI_SSID", lookup_secret("wifi", "ssid")),
    ("WIFI_PASSWORD", lookup_secret("wifi", "password")),
    ("MQTT_USER", lookup_secret("mqtt", "user")),
    ("MQTT_PASSWORD", lookup_secret("mqtt", "password"))
])

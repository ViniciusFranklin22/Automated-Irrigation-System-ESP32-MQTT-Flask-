import pandas as pd
import paho.mqtt.client as mqtt
import json
import os
import threading

csv_path = 'C:\\Users\\vinic\\Documents\\Projetos_ESP32\\Laboratório de Projetos Eletrônicos\\Testes MQTT\\MQTT\\irrigacao.csv'

BROKER = ""
PORT = 1883
TOPIC_RECEIVE = "esp32-001/inicio_irrigacao"

# 🔒 lock para evitar escrita simultânea
lock = threading.Lock()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Conectado ao broker MQTT!")
        client.subscribe(TOPIC_RECEIVE, qos=1)
        print(f"📥 Inscrito no tópico: {TOPIC_RECEIVE}")
    else:
        print(f"❌ Falha na conexão. Código de erro: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)

        if not isinstance(data, dict):
            print("⚠️ Payload inválido")
            return

        df = pd.DataFrame([data])
        df["irrigacao_em_curso"] = int(df["irrigacao_em_curso"])
        df["irrigacao_finalizada"] = int(df["irrigacao_finalizada"])

        # 🔥 escrita eficiente (append)
        with lock:
            df.to_csv(
                csv_path,
                mode='a',  # append
                header=not os.path.exists(csv_path),
                index=False
            )

        print(f"✅ Dados salvos: {data}")

    except Exception as e:
        print(f"⚠️ Erro ao processar mensagem: {e}")

client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

print("🔌 Conectando ao broker...")
client.connect(BROKER, PORT, keepalive=60)

client.loop_forever()
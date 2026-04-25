from flask import Flask, request, url_for, session, redirect, render_template, flash
from paho.mqtt import client as mqtt_client
import os
import pandas as pd
import datetime
import time
from flask import jsonify
import time


app = Flask(__name__)

app.config['SESSION_COOKIE_NAME'] = 'Projetos'
app.secret_key = 'vascodagama1898**/1519xc1e98d498vqw1f98'
TOKEN_INFO = 'token_info'
csv_path = 'C:\\Users\\vinic\\Documents\\Projetos_ESP32\\Laboratório de Projetos Eletrônicos\\Testes MQTT\\MQTT\\data.csv'
csv_irrigacao_path = 'C:\\Users\\vinic\\Documents\\Projetos_ESP32\\Laboratório de Projetos Eletrônicos\\Testes MQTT\\MQTT\\irrigacao.csv'

msg = "Regar Vaso Agora"

TEMPO_ESPERA_HARDWARE = 4 # tempo de espera para alterar uma informação no hardware para espelhar aqui


BROKER = ""
PORT = 1883
topic_send = "esp32-001/receive"
topic_config_thshold = "esp32-001/thshold"
topic_config_min_entre_irrigacao = "esp32-001/min_entre_irrigacao"
topic_config_tempo_irrigacao = "esp32-001/tempo_irrigacao"
topic_bool_irrigacao_auto = "esp32-001/auto_mode"
client_id = f'publish-flask'

planta_irrigada = 0
time_break = 20

# enviar para topico

def connect_mqtt():
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
            client.subscribe(topic_send,qos=1)
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(client_id=client_id, callback_api_version=mqtt_client.CallbackAPIVersion.VERSION1)
    client.on_connect = on_connect
    client.connect(BROKER, PORT)
    return client

def publish(client):
    result = client.publish(topic_send, msg)
    status = result[0]
    if status == 0:
        print(f"Send `{msg}` to topic `{topic_send}`")
    else:
        print(f"Failed to send message to topic {topic_send}")

# Função chamada quando uma mensagem é recebida
def on_message(client, userdata, msg):
    global planta_irrigada
    try:
        payload = msg.payload.decode('utf-8')
        print(payload)
        if int(payload) == 1:
            planta_irrigada = 1

    except Exception as e:
        print(f"⚠️ Erro ao processar mensagem: {e}")



def wait_for_irrigation_confirmation(timeout=0):
    global planta_irrigada
    planta_irrigada = 0
    start = time.time()
    while time.time() - start < timeout:
        if planta_irrigada == 1:
            return True
        time.sleep(0.1)
    return False


def send_data():
    client = connect_mqtt()
    client.loop_start()
    client.on_message = on_message
    publish(client)
    success = wait_for_irrigation_confirmation()
    client.loop_stop()


def get_current_config():

    try:
        df = pd.read_csv(csv_path)

        last = df.iloc[-1]

        soil_min = int(last["threshold_humidity"])
        intervalo = int(last["tempo_min_entre_irrigacoes"]/60000)
        tempo_bomba = int(last["TEMPO_RELE_ATIVO"]/1000)
        bool_irrig_auto = last["bool_irrigacao_auto"]
        global time_break 
        time_break = tempo_bomba
        return soil_min, intervalo, tempo_bomba, bool_irrig_auto

    except Exception as e:
        print("Erro lendo config:", e)

        return 0,0,0,0

@app.route('/')
def index():

    soil_min, intervalo, tempo_bomba, bool_irrig_auto = get_current_config()

    return render_template(
        "index.html",
        soil_min=soil_min,
        intervalo=intervalo,
        tempo_bomba=tempo_bomba,
        bool_irrig_auto = bool_irrig_auto


    )

@app.route('/data')
def get_data():
    try:
        range_h = int(request.args.get("range", 24))
        now = datetime.datetime.now()

        limite = range_h * 3600  # periodo de amostragem ta por volta de 1s, entao otimiza a coleta de dados, pra nao pegar a mais
        df = pd.read_csv(csv_path, low_memory=False).tail(limite)
        irrig = pd.read_csv(csv_irrigacao_path)
        #print(df.head(1))
        #print(df.tail(1))
        df['timestamp'] = pd.to_datetime(df['timestamp'], format="%d/%m/%Y %H:%M:%S")
        
        
        recent = df[df['timestamp'] > now - datetime.timedelta(hours=range_h)]

        ultima_linha = recent.tail(1)
        #print(recent.head(1))
        #print(ultima_linha)

        MAX_POINTS = 1000

        if len(recent) > MAX_POINTS:
            step = len(recent) // MAX_POINTS
            recent = recent.iloc[::step]

        recent = pd.concat([recent, ultima_linha])
        
        #print(recent.tail(1))
        # ordena (importante pro gráfico)
        recent = recent.sort_values(by='timestamp')
        #print(recent.tail(1))
        labels = recent['timestamp'].dt.strftime("%d/%m %H:%M:%S").tolist()
        soil = recent['soil_humidity'].tolist()
        ldr = recent['ldr_value'].tolist()
        temp = recent['temp_value'].to_list()
        umid = recent['umid_value'].to_list()
        #ultima_irrigacao =  recent['ultima_irrigacao'].to_list()
        ultima = irrig.loc[irrig['irrigacao_finalizada'] == 1, "timestamp_fim"].tail(1)

        if not ultima.empty:
            ultima_irrigacao = pd.to_datetime(ultima.iloc[0],format="%d/%m/%Y %H:%M:%S").strftime("%d/%m %H:%M:%S")
        else:
            ultima_irrigacao = None


        if not irrig.empty:
            irrigando = int(irrig.iloc[-1]["irrigacao_em_curso"])
        else:
            irrigando = 0
    

        return jsonify({
            "labels": labels,
            "soil": soil,
            "ldr": ldr,
            "temp": temp,
            "umid": umid,
            'ultima_irrigacao':ultima_irrigacao,
            "irrigando": irrigando
        })
    except Exception as e:
        print(f"Erro ao carregar dados: {e}")
        return jsonify({"labels": [], "soil": [], "ldr": []})
    


@app.route('/config/soil_min', methods=['POST'])
def config_soil_min():

    valor = request.form.get('valor')

    if not valor:
        flash("⚠️ Digite um valor.", "warning")
        return redirect(url_for("index"))

    try:
        valor = int(valor)
    except ValueError:
        flash("⚠️ O valor precisa ser um número inteiro.", "danger")
        return redirect(url_for("index"))

    if valor < 0:
        flash("⚠️ O valor precisa ser positivo.", "danger")
        return redirect(url_for("index"))

    if valor > 4095:
        flash("⚠️ O sensor do ESP32 vai apenas até 4095.", "danger")
        return redirect(url_for("index"))
    
    try:
        client = connect_mqtt()
        client.loop_start()
        client.publish(topic_config_thshold, f"{valor}")
        client.loop_stop()
        time.sleep(TEMPO_ESPERA_HARDWARE)
        flash("✅ Umidade mínima atualizada!", "success")
    except Exception as e:
        flash(f"Erro ao se conectar com o Sistema {e}", "danger")
    
    return redirect(url_for("index"))


@app.route('/config/intervalo', methods=['POST'])
def config_intervalo():

    valor = request.form.get('valor')

    if not valor:
        flash("⚠️ Digite um valor.", "warning")
        return redirect(url_for("index"))

    try:
        valor = int(valor)
    except ValueError:
        flash("⚠️ O valor precisa ser um número inteiro.", "danger")
        return redirect(url_for("index"))

    if valor <= 0:
        flash("⚠️ O valor precisa ser positivo.", "danger")
        return redirect(url_for("index"))

    if valor < 10:
        flash("⚠️ O intervalo mínimo permitido é 10 minutos.", "danger")
        return redirect(url_for("index"))

    try:
        client = connect_mqtt()
        client.loop_start()
        client.publish(topic_config_min_entre_irrigacao, f"{valor*60000}")
        client.loop_stop()
        time.sleep(TEMPO_ESPERA_HARDWARE)
        flash("✅ Intervalo atualizado!", "success")
    except Exception as e:
        flash(f"Erro ao se conectar com o Sistema {e}", "danger")

    return redirect(url_for("index"))


@app.route('/config/tempo_bomba', methods=['POST'])
def config_tempo_bomba():

    valor = request.form.get('valor')

    if not valor:
        flash("⚠️ Digite um valor.", "warning")
        return redirect(url_for("index"))

    try:
        valor = int(valor)
    except ValueError:
        flash("⚠️ O valor precisa ser um número inteiro.", "danger")
        return redirect(url_for("index"))

    if valor <= 0:
        flash("⚠️ O valor precisa ser positivo.", "danger")
        return redirect(url_for("index"))

    if valor > 120:
        flash("⚠️ Tempo máximo de irrigação permitido é 120 segundos.", "danger")
        return redirect(url_for("index"))

    try:
        client = connect_mqtt()
        client.loop_start()
        client.publish(topic_config_tempo_irrigacao, f"{valor*1000}")
        client.loop_stop()
        time.sleep(TEMPO_ESPERA_HARDWARE)
        flash("✅ Tempo de irrigação atualizado!", "success")
    except Exception as e:
        flash(f"Erro ao se conectar com o Sistema {e}", "danger")

    return redirect(url_for("index"))



@app.route('/config/auto_irrigacao', methods=['POST'])
def config_bool_irrig_auto():

    valor = request.form.get('valor')

    if valor == "1":
        valor = 1
    else:
        valor = 0

    try:
        client = connect_mqtt()
        client.loop_start()
        time.sleep(TEMPO_ESPERA_HARDWARE)
        client.publish(topic_bool_irrigacao_auto, valor)
        client.loop_stop()

        if valor == 1:
            flash("✅ Irrigação automática habilitada!", "success")
        else:
            flash("🚫 Irrigação automática desabilitada!", "success")

    except Exception as e:
        flash(f"Erro ao se conectar com o sistema: {e}", "danger")

    return redirect(url_for("index"))

@app.route('/regar', methods=['GET'])
def regar():
    send_data()
    session['atualizar'] = True
    return redirect(url_for("index"))

    

@app.route('/status')
def status():
    try:
        df = pd.read_csv(csv_irrigacao_path)
        irrigando = int(df.iloc[-1]['irrigacao_em_curso'])
        return jsonify({"irrigando": irrigando})
    except:
        return jsonify({"irrigando": 0})

#usar quando for para rodar na sub-rede
if __name__ == '__main__':
    #app.run(host='0.0.0.0', port=5000, debug=True,use_reloader=False) # running in the subnet
    app.run(debug=True, port=5000,use_reloader=False)
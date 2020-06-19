#include <CTBot.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

CTBot raBot;

const char* ssid  = ""; // Nome do Wi-Fi
const char* pass  = "";  // Senha do Wi-Fi

const char* mqttServer = "broker.mqtt-dashboard.com"; // Rota do broker MQTT. Local: '127.0.0.1'. Remoto: 'broker.mqtt-dashboard.com' (broker publico, apenas para testes).
const int mqttServerPort = 1883; // Porta do broker MQTT. Local: 1883. Remoto: '1883'.

uint32_t telegramUserId = ;//  Id do usuario do Telegram
const String telegramToken = "";  // Token do bot do Telegram

//  Define pinos de sensor e atuadores
int PIR = D2;
int pirLED = D1;
int loudSpeaker = D3;

//  Define valores iniciais para as variáveis de monitoramento interno do PIR
int pirValue = LOW;
int previousPirValue = LOW;

//  Define flags de atuadores inicialmente como false
bool isPirLEDOn = false;
bool isLoudSpeakerOn = false;
bool isMuted = false;

TBMessage msg; //  Mensagem do Telegram a ser recebida 

WiFiClient espClient; // Define cliente WiFi
PubSubClient client(espClient); // Define cliente MQTT (rodando sobre TCP/IP)

String hello = "Olá e bem vindo ao RemoteAlert!\nVocê será notificado sempre que detectarmos novos movimentos. Também serão acionados o LED de alerta e o alto falante do sistema durante a presença dos movimentos.\nPara acender o led de alerta, envie 'Acender LED de alerta'. Para apagá-lo, envie 'Apagar LED de alerta'.\nPara ligar o alarme sonoro do sistema, envie 'Ligar alarme sonoro'. Para desligá-lo, envie 'Desligar alarme sonoro'.\nPara ativar/desativar o modo silencioso, utilize o dashboard do sistema; note que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.";

//  Conecta ESP8266 ao WiFi
void setupWiFi() 
{
  //  Imprime ssid no monitor serial
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  //  WiFi.mode(WIFI_STA); // Define WiFi como estatico
  
  WiFi.begin(ssid, pass); // Inicia rotina de conexao ao WiFi

  //  Imprime '.' enquanto a conexao nao for estabelecida
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //  Imprime IP do ESP8266 agora conectado ao WiFi
  Serial.println("");
  Serial.print("WiFi conectado! - Endereco de IP do ESP8266: ");
  Serial.println(WiFi.localIP());

  delay(10); // Espera 10 milissegundos
}

//  Rotina de callback (o que fazer quando receber mensagem MQTT)
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Mensagem recebida no topico: ");
  Serial.print(topic);
  Serial.print(". Mensagem: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if(String(topic) == String("remoteAlert/modoSilencioso"))
  {
    if(messageTemp == "on")
    {
      if(isMuted)
      {
       Serial.println("Modo silencioso do sistema ja ativado.");
      }
      else
      {
       Serial.println("Ativando modo silencioso do sistema.");
       isMuted = true;
       raBot.sendMessage(telegramUserId, "RemoteAlert está agora em seu modo silencioso! Para desativar o modo, utilize o dashboard do sistema.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
    }
    else if(messageTemp == "off")
    {
      if(isMuted)
      {
       Serial.println("Desativando modo silencioso do sistema.");
       isMuted = false;
       raBot.sendMessage(telegramUserId, "Modo silencioso desativado! Para ativar o modo, utilize o dashboard do sistema.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
  
      }
      else
      {
       Serial.println("Modo silencioso do sistema ja desativado.");
      }
    }

    
  }
}

//  Refaz coneccao do ESP8266 com broker MQTT
void reconnectMQTT()
{
  //  Executa rotina enquanto cliente nao estiver conectado ao broker
  while (!client.connected())
  {
    Serial.print("Tentando conectar cliente ao broker MQTT...");
    
    //  WiFi.mode(WIFI_STA); // Define WiFi como estatico

    //  Cria id randomico para o cliente
    String clientId = "ESP8266Client-" + String(random(0xffff), HEX);

    // Tenta se conectar e, caso consiga, se inscreve no topico 'remoteAlert/modoSilencioso'
    if(client.connect(clientId.c_str()))
    {
      Serial.println("conectado");  
      
      client.subscribe("remoteAlert/modoSilencioso");
    } 
    //  Imprime mensagem de erro, caso contrario
    else 
    {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println("tente novamente em 2 segundos");
      
      delay(2000);
    }
  }
}

void setup()
{
  //  Inicializa monitor serial
  Serial.begin(9600);
  Serial.println("\nInicializando RemoteAlert...");
  
  // Conecta ESP8266 ao WiFi
  setupWiFi(); 
  
  //  Define broker MQTT e rotina de callback para mensagens recebidas
  client.setServer(mqttServer, mqttServerPort);
  client.setCallback(callback);

  //  Inicializa token do bot Telegram
  raBot.setTelegramToken(telegramToken);

  //  Testa conecção com servidor Telegram
  if (raBot.testConnection())
    Serial.println("Bot Telegram conectado com sucesso!");
  else
    Serial.println("Falha na coneccao, bot Telegram nao conectado!");

   //  Prepara E/S
   pinMode(pirLED, OUTPUT);
   pinMode(PIR, INPUT);

   raBot.sendMessage(telegramUserId, hello); // Envia mensagem do Telegram inicial ao usuario 
}
void loop()
{ 
  //  Valida conexão com cliente MQTT e reconecta caso necessario
  if (!client.connected()) {
    reconnectMQTT();
  }

  if (!client.loop()) {
    reconnectMQTT();
  }
  
  //  Lê sensor PIR e atualiza variáveis
  previousPirValue = pirValue;
  pirValue = digitalRead(PIR);
  
  //  Envia mengagem ao usuário via Telegram caso novo movimento tenha sido detectado
  if (pirValue == HIGH && previousPirValue == LOW)
  {
    raBot.sendMessage(telegramUserId, "Alerta de movimento!");
  }

  //  Envia mensagem ao usuário via Telegram caso movimentos deixem de ser detectados
  else if (pirValue == LOW && previousPirValue == HIGH)
  {
    raBot.sendMessage(telegramUserId, "Alerta de movimento encerrado!");
  }

  //  Ativa flag do LED de alerta e envia mensagem ao usuário via Telegram novo movimento tenha sido detectado
  if (!isPirLEDOn && pirValue == HIGH && previousPirValue == LOW) 
  {
    isPirLEDOn = true;
    raBot.sendMessage(telegramUserId, "LED de alerta aceso!");
  }
  
  //  Desativa flag do LED de alerta e envia mensagem ao usuário via Telegram caso movimentos deixem de ser detectados
  else if (isPirLEDOn && pirValue == LOW && previousPirValue == HIGH) 
  {
    isPirLEDOn = false;
    raBot.sendMessage(telegramUserId, "LED de alerta apagado");
  }

  //  Ativa flag do alarme sonoro e envia mensagem ao usuário via Telegram caso novo movimento tenha sido detectado e o sistema não esteja no modo silencioso
  if (!isMuted && !isLoudSpeakerOn && pirValue == HIGH && previousPirValue == LOW)
  {
    isLoudSpeakerOn = true;
    raBot.sendMessage(telegramUserId, "Alarme sonoro ligado!");

  }

  //  Desativa flag do alarme sonoro e envia mensagem ao usuário via Telegram caso movimentos deixem de ser detectados e o sistema não esteja no modo silencioso
  else if (!isMuted && isLoudSpeakerOn && pirValue == LOW && previousPirValue == HIGH)
  {
    isLoudSpeakerOn = false;
    raBot.sendMessage(telegramUserId, "Alarme sonoro desligado!");
  }

  //  Acende LED de alerta caso sua flag esteja ativada ou apaga-o caso contrário
  if(isPirLEDOn)
  {
    digitalWrite(pirLED, HIGH);
  }
  else
  {
    digitalWrite(pirLED, LOW);
  }

  //  Liga alarme sonoro do sistema caso sua flag esteja ativa ou desliga-o caso contrário
  if(isLoudSpeakerOn)
  {
    tone(loudSpeaker, 1200, 900);
  }

  //  Checa novas mensagens do Telegram 
  if (raBot.getNewMessage(msg) && msg.sender.id == telegramUserId)
  {    
    //  Verifica se nova mensagem é, ignorando capitalização, "Acender LED de alerta"
    if (msg.text.equalsIgnoreCase("Acender LED de alerta"))
    {
      //  Envia mensagem caso o LED já esteja aceso (flag ativada)
      if(isPirLEDOn)
      {
        raBot.sendMessage(telegramUserId, "LED de alerta já aceso! Para apagá-lo, envie 'Apagar LED de alerta'");
      }
      //  Caso contrário, ativa flag e envia mensagem
      else
      {
        isPirLEDOn = true;
        raBot.sendMessage(telegramUserId, "LED de alerta aceso!");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Apagar LED de alerta"
    else if (msg.text.equalsIgnoreCase("Apagar LED de alerta"))
    {
      //  Desativa flag do LED e envia mensagem, caso LED esteja aceso (flag ativada)
      if(isPirLEDOn)
      {
        isPirLEDOn = false;
        raBot.sendMessage(telegramUserId, "LED de alerta apagado!");
      }
      //  Caso contrário, envia mensagem 
      else
      {
        raBot.sendMessage(telegramUserId, "LED de alerta já apagado! Para acendê-lo, envie 'Acender LED de alerta'");
      }    
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Ligar alarme sonoro"
    else if (msg.text.equalsIgnoreCase("Ligar alarme sonoro"))
    {
      //  Envia mensagem ao usuário caso o sistema esteja em seu modo silencioso
      if(isMuted)
      {
        raBot.sendMessage(telegramUserId, "RemoteAlert está em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Envia mensagem ao usuário caso o sistema não esteja no modo silencioso, mas o alarme sonoro já esteja ligado
      else if (isLoudSpeakerOn)
      {
        raBot.sendMessage(telegramUserId, "Alarme sonoro já ligado! Para desligá-lo, envie 'Desligar alarme sonoro'.");

      }
      //  Caso contrário, liga o alarme sonoro e envia mensagem ao usuário
      else
      {
        isLoudSpeakerOn = true;
        raBot.sendMessage(telegramUserId, "Alarme sonoro ligado!");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Desligar alarme sonoro"
    else if (msg.text.equalsIgnoreCase("Desligar alarme sonoro"))
    {
      //  Envia mensagem ao usuário caso o sistema esteja em seu modo silencioso
      if(isMuted)
      {
        raBot.sendMessage(telegramUserId, "RemoteAlert está em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Desliga o alarme sonoro e envia mensagem ao usuário, caso o sistema não esteja em seu modo silencioso e o alarme sonoro esteja ligado
      else if (isLoudSpeakerOn)
      {
        isLoudSpeakerOn = false;
        raBot.sendMessage(telegramUserId, "Alarme sonoro desligado!");
      }
      //  Envia mensagem ao usuário caso contrário
      else
      {
        raBot.sendMessage(telegramUserId, "Alarme sonoro já desligado! Para ligá-lo, envie 'Ligar alarme sonoro'.");
      }
    }

    //  Envia resposta genérica para qualquer outra mensagem
    else
    {
      raBot.sendMessage(telegramUserId, hello);
    }
  }

  //  Publica mensagem contendo valor atual do PIR no tópico remoteAlert/alertaMovimento
  client.publish("remoteAlert/alertaMovimento", "aaaaaaaaaaa");
  
  //  Espera 0.5 segundos
  delay(500);
}

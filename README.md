# RemoteAlert
Projeto semestral da disciplina de Objetos Inteligentes Conectados, turma 05K11. 

Integrantes:
* Henrique Araujo Macadura
* Lucas Barbosa de Figueiredo

Primeiro semestre de 2020.

## Funcionamento e Uso
Neste projeto, desenvolvemos um sistema de detecção e alerta de movimentos baseado em um sensor PIR e um módulo nodeMCU ESP8266. 

O sistema irá alertar o usuário sempre que detectar novos movimentos através de mensagem via Telegram, ao mesmo tempo em que um LED amarelo e um buzzer são acionados durante a detecção do mesmo. Também via Telegram, o usuário poderá ativar tanto o LED de alerta quando o alarme sonoro do sistema de forma independente a qualquer instante, através do envio de mensagens específicas.

* __Acender LED de alerta__ para acender o LED amarelo, caso o mesmo se encontre apagado.
* __Apagar LED de alerta__ para apagá-lo, caso esteja aceso.
* __Ligar alarme sonoro__ para ativar o alarme do buzzer do sistema, caso o mesmo se encontre inativo.
* __Desligar alarme sonoro__ para desativá-lo, caso esteja "tocando".

O sistema também conta com um _dashboard_ contendo um gráfico indicando o estado do sensor de movimento nas últimas 24hs e um botão para ativar e desativar o modo silencioso do sistema - no qual o alarme sonoro está sempre desativado e, quando ocorre nova detecção de movimento, são acionados apenas o LED de alerta e a mensagem do Telegram. O _dashboard_ foi implementado no Node-RED, se comunicando com o sistema via protocolo MQTT utilizando o _broker_ público __broker.mqtt-dashboard.com__.

Um vídeo demonstrando o sistema em funcionamento se encontra disponível em https://youtu.be/PSYkpyM_V9E.

## Software
### Bibliotecas
Utilizamos 3 bibliotecas no projeto: 
1. __CTBot__, para uso do bot do Telegram, 
2. __ESP8266WiFi__, para conexão com internet via WiFi e
3. __PubSubClient__, para uso do protocolo MQTT.
```ino
#include <CTBot.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
```
### Variáveis Globais
Primeiramente, definimos bot do Telegram a ser configurado e utilizado posteriormente.
```ino
CTBot raBot;
```
Para conexão com a internet, precisamos do WiFi e sua senha.
```ino
const char* ssid  = ""; // Nome do Wi-Fi
const char* pass  = "";  // Senha do Wi-Fi
```
Definimos o endereço e a porta do servidor (_broker_) MQTT ao qual iremos conectar o ESP8266, então definimos um cliente do tipo WiFiClient e o especializamos utilizando os tipos da biblioteca PubSubClient para que opere como cliente MQTT.
```ino
const char* mqttServer = "broker.mqtt-dashboard.com"; // Rota do broker MQTT. Local: '127.0.0.1'. Remoto: 'broker.mqtt-dashboard.com' (broker publico, apenas para testes).
const int mqttServerPort = 1883; // Porta do broker MQTT. Local: 1883. Remoto: '1883'.

WiFiClient espClient; // Define cliente WiFi
PubSubClient client(espClient); // Define cliente MQTT (rodando sobre TCP/IP)
```
Para uso do Telegram, definimos o id do usuário que receberá as notificações e poderá controlar o sistema via mensagens, o token do bot criado, uma variável do tipo TBMessage para armazenar a última mensagem recebida e uma String contendo uma mensagem inicial com instruções para utilizar o sistema via Telegram.
```ino
uint32_t telegramUserId = ;//  Id do usuario do Telegram
const String telegramToken = "";  // Token do bot do Telegram

TBMessage msg; //  Mensagem do Telegram a ser recebida 

String hello = "Olá e bem vindo ao RemoteAlert!\nVocê será notificado sempre que detectarmos novos movimentos. Também serão acionados o LED de alerta e o alto falante do sistema durante a presença dos movimentos.\nPara acender o led de alerta, envie 'Acender LED de alerta'. Para apagá-lo, envie 'Apagar LED de alerta'.\nPara ligar o alarme sonoro do sistema, envie 'Ligar alarme sonoro'. Para desligá-lo, envie 'Desligar alarme sonoro'.\nPara ativar/desativar o modo silencioso, utilize o dashboard do sistema; note que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.";

```
Por fim, definimos variáveis para uso do sensor e dos atuadores do sistema, incluindo os pinos aos quais estão conectados, variáveis para armazenar seu valor, _flags_ para controle interno e variáveis para medição de tempo em milissegundos.
```ino
//  Define pinos de sensor e atuadores
int PIR = D2;
int pirLED = D1;
int loudSpeaker = D3;

//  Define valores iniciais para as variáveis de monitoramento interno do PIR
int pirValue = LOW;
int previousPirValue = LOW;

//  Define valores iniciais para as variáveis de tempo
long currentTimeMeasure = millis();
long lastTimeMeasure = 0;

//  Define flags de atuadores inicialmente como false
bool isPirLEDOn = false;
bool isLoudSpeakerOn = false;
bool isMuted = false;
```
### Funções Auxiliares
A função __setupWifi__ é utilizada para garantir conexão do ESP8266 com a internet, imprimindo o processo da conexão e seu resultado no monitor serial.
```ino
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
```
Aqui definimos o __callback__ executado após o recebimento de mensagens via MQTT (quando forem publicadas mensagens em tópicos nos quais o ESP8266 estiver inscrito). 

Os parâmetros da função são padrões predefinidos para o uso da __PubSubClient__ e, neste caso, implementamos o corpo da função para comparar o tópico passados como parâmetro e o payload (mensagem publicada) - com seus tipos já propriamente convertidos - a "remoteAlert/modoSilencioso" (tópico no qual inscreveremos o ESP8266) e "on" ou "off", atualizando a variável que controla o modo silencioso do sistema, notificando o usuário via Telegram e imprimindo processo e resultados da rotina no monitor serial.
```ino
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
```
Utilizamos __reconnectMQTT__ para garantir a conexão do cliente MQTT delcarado para ESP822 com o broker previamente definido.

A rotina de conexão é executada em _loop_ até que ocorra com sucesso. Processo e resultados da conexão são impressos no monitor serial.
```ino
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
```

### Setup
Dentro do __setup__, chamamos funções previamente declaradas para inicializar todas as funcionalidades do sistema, incluindo conexão com internet e com o _broker_ MQTT, conexão com servidor do Telegram para utilização do bot e E/S através dos sensores e atuadores do sistema.

Por fim, uma mensagem inicial é enviada para o usuário via Telegram.
```ino
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
```
### Loop Principal
Dentro do __loop__ principal do sistema, começamos por garantir a conexão do ESP8266 com o servidor MQTT e executando seus processos definidos dentro do __callback__.

Em seguida, fazemos uma leitura do sensor PIR atualizando ambas suas variáveis e, caso seu valor seja diferente da última leitura, alertamos o usuário via Telegram e alteramos as _flags_ de controle interno de acordo.

Com as _flags_ tendo seus valores corretamente atribuídos, acionamos (ou não) os atuadores corretamente.

Uma leitura do tempo atual em milissegundos é feita e, caso tenham se passado 5 segundos desde a última marcação, uma mensagem contendo o valor atual do PIR é publicada no tópico "remoteAlert/alertaMovimento" via MQTT e a última marcação é atualizada.

Por fim, o bot verifica se recebeu uma nova mensagem e, se sim, compara seu valor com as mensagens predefinidas ("Acender LED de alerta", "Apagar LED de alerta", "Ligar alarme sonoro" ou "Desligar alarme sonoro") e atualiza suas variáveis de acordo com a mensagem recebida, enviando uma mensagem de resposta ao usuário informando as mudanças realizadas. Caso o usuário envie uma mensagem não prevista pelo sistema, a mensagem inicial contendo instruções é enviada como resposta. Caso outro usuário tente se comunicar com o bot, sua mensagem não é considerada pelo sistema.
```ino
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

  //  Atualiza variáveis e publica valor do PIR a cada 5 segundos
  currentTimeMeasure = millis();
  if (currentTimeMeasure - lastTimeMeasure > 5000)
  {
    lastTimeMeasure = currentTimeMeasure;
    
    if (pirValue == HIGH)
      client.publish("remoteAlert/alertaMovimento", "1");
    else
      client.publish("remoteAlert/alertaMovimento", "0");
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
  
  //  Espera 0.5 segundos
  delay(500);
}
```

## Hardware
O sistema foi construído com os seguintes materiais:
* 01 x NodeMCU ESP2866
* 01 x Sensor PIR
* 01 x Buzzer simples
* 01 x LED amarelo
* 02 x Resistor 330 ohms
* 01 x Protoboard 400 pontos
* 01 x Jumper macho-macho
* 10 x Jumper macho-fêmea

Seguindo o circuito abaixo como modelo:
![Circuito fritzing](/img/circuitoRemoteAlert.jpeg)

A alimentação do sistema - via sua porta micro USB tipo B - foi realizada plugando-o ao mesmo notebook no qual o software foi desenvolvido na IDE do Arduino e o dashboard do Node-RED foi acessado.

## Interfaces, Protocolos e Módulos de Comunicação
### Telegram Bot
Utilizamos a API do Telegram para criar nosso bot, a partir do _BotFather_.
Procurando pelo mesmo dentro do telegram, podemos acioná-lo clicando em __START__ após abrir uma conversa com ele.
Em seguida, enviando o comando __/newbot__, damos início a criação do bot que iremos utilizar para contatar o sistema. 
Enviamos, então, o nome que iremos dar ao nosso bot (no nosso caso, __a__) e seu nome de usuário do Telegram ().
![Telegram Bot Creation](/img/telegramBotCreation.jpeg)

Por fim, recebemos um token do bot criado com sucesso e podemos contatá-lo buscando seu nome dentro do Telegram e enviando __/start__.
![Telegram Bot Created](/img/telegramBotCreated.jpeg)

![Telegram Bot Chat](/img/telegramBotChat.jpeg)

Tendo nosso bot, podemos utilizar a biblioteca CTBot para instanciá-lo (criando um objeto identificado pelo mesmo token do bot criado através do _BotFather_) e utilizar suas funcionalidades básicas de envio e recebimento de mensagens.

Como temos acesso ao conteúdo das mensagens recebidas, podemos realizar tarefas dentro do sistema acionadas via comandos enviados pelo usuário no Telegram comparando tal conteúdo com Strings predefinidas e mapeadas a ações específicas (neste caso, a controlar os atuadores do sistema).

O bot também foi utilizado para enviar mensagens para o usuário, não somente oferecendo-o um feedback sobre a execução dos comandos que o mesmo enviar ao sistema, mas também servindo como um alerta remoto do sistema caso o sensor PIR detecte algum movimento novo.

Obs.: Optamos por verificar também o id do usuário que enviou a mensagem ao bot para que o sistema não possa receber comandos de usuários desconhecidos. Isto foi feito acessando as propriedades do objeto de mensagem (TBMessage) recebido e identificado pelo nosso bot (CTBot).

### MQTT
O protocolo MQTT (implementado em cima do protocolo TCP/IP) foi utilizado para comunicação do sistema com seu dashboard do Node-RED via a internet. 

A implementação foi feita utilizando a biblioteca PubSubClient, que nos permitiu definir o _broker_ a ser utilizado, declarar um cliente para o ESP8288, conectá-los e realizar as operações de publicação e inscrição em tópicos.

Os tópicos utilizados foram:
1. __remoteAlert/alertaMovimento__: neste tópico, no qual inscrevemos parte do fluxo do Node-RED, o ESP8266 publica uma mensagem contendo o estado atual do sensor PIR (1 se estiver detectando movimento e 0 caso contrário) de 5 em 5 segundos, permitindo a criação de um gráfico da atividade do sistema que mantém um histórico das últimas 24 horas.
2. __remoteAlert/modoSliencioso__: neste tópico, no qual inscrevemos o ESP8266, utilizamos um botão do _dashboard_ do Node-RED para publicar 
uma mensagem referente a ativação do modo silencioso do sistema ("on" para ativado, "off" para desativado).

### Node-RED
Para comunicação com o sistema via MQTT, optamos por construir um fluxo com a ferramenta Node-RED para gerar um _dashboard_ contendo o gráfico das últimas 24hs de atividade do sistema e também um botão para controlar o modo silencioso do mesmo.

Com o Node-RED rodando localmente na porta 1880, acessamos sua interface web, importamos a biblioteca __node-red-dashboard__ através de seu gerenciador, e construímos o seguinte fluxo:
![Node-RED flow](/img/nodeREDFlow.png)

Configuramos, para sua demonstração, o broker público anteriormente mencionado
![Node-RED broker](/img/nodeREDBroker.png)

e configuramos seus nós da seguinte forma:

1. Botão liga-desliga do modo silencioso
![Node-RED switch node](/img/nodeREDSwitch.png)

2. Saída MQTT para o tópico remoteAlert/modoSliencioso
![Node-RED MQTT out node](/img/nodeREDBMQTTOut.png)

3. Entrada MQTT para o tópico remoteAlert/alertaMovimento
![Node-RED MQTT in](/img/nodeREDMQTTIn.png)

4. Gráfico contendo as últimas 24hs dos valores detectados pelo sensor PIR do sistema
![Node-RED chart node](/img/nodeREDChart.png)

Após o _deploy_ podemos acessar sua interface gráfica (_dashboard_), que tem a seguinte forma:
![Node-RED dashboard](/img/nodeREDDashboard.png)

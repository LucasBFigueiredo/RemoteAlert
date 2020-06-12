#include "CTBot.h"

CTBot raBot;

String ssid  = ""; // Colocar nome do wifi e apagar comentario
String pass  = "";  // Colocar senha do wifi e apagar comentario
String token = "";  // Colocar token do bot e apagar comentario

int PIR = D2;
int pirLED = D1;
int loudSpeaker = D3;
int pirValue = LOW;
int previousPirValue = LOW;

bool isPirLEDOn = false;

TBMessage msg; //  Mensagem do Telegram a ser recebida 

String hello = "Olá e bem vindo ao RemoteAlert!\nVocê será notificado sempre que detectarmos novos movimentos. Também será acionado o LED de alerta durante a presença dos movimentos.\nPara acender o led de alerta, envie 'Acender LED de alerta'. Para apagá-lo, envie 'Apagar LED de alerta'.";

void setup()
{
  //  Inicializa monitor serial
  Serial.begin(115200);
  Serial.println("\nInicializando RemoteAlert...");

  //  Conecta ao WiFi
  raBot.wifiConnect(ssid, pass);

  //  Inicializa token do Telegram
  raBot.setTelegramToken(token);

  //  Testa conecção
  if (raBot.testConnection())
    Serial.println("Conecção realizada com sucesso!");
  else
    Serial.println("Falha na conecção!");

  //  Prepara E/S
  pinMode(pirLED, OUTPUT);
  pinMode(PIR, INPUT);

  //  Envia mensagem inicial
  raBot.sendMessage(msg.sender.id, reply);
}

void loop()
{  
  //  Lê sensor PIR e atualiza variáveis
  previousPirValue = pirValue;
  pirValue = digitalRead(PIR);

  //  Imprime no motor serial os estados atuais do 
  Serial.prinln("\nera: ");
  Serial.printf("%d",previousPirValue);
  Serial.println("agora é:");
  Serial.printf("%d",pirValue);
  
  //  Envia mengagem ao usuário caso novo movimento tenha sido detectado
  if (pirValue == HIGH && previousPirValue == LOW)
  {
    raBot.sendMessage(msg.sender.id, "Alerta de movimento!");
  }

  //  Envia mensagem ao usuário caso movimentos deixem de ser detectados
  else if (pirValue == LOW && previousPirValue == HIGH)
  {
    raBot.sendMessage(msg.sender.id, "Alerta de movimento encerrado!");
  }

  //  Ativa flag do LED de alerta e envia mensagem ao usuário novo movimento tenha sido detectado
  if (!isPirLEDOn && pirValue == HIGH && previousPirValue == LOW) 
  {
    isPirLEDOn = true;
    raBot.sendMessage(msg.sender.id, "LED de alerta aceso!");
  }
  
  //  Desativa flag do LED de alerta e envia mensagem ao usuário caso movimentos deixem de ser detectados
  else if (isPirLEDOn && pirValue == LOW && previousPirValue == HIGH) 
  {
    isPirLEDOn = false;
    raBot.sendMessage(msg.sender.id, "LED de alerta apagado");
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

  //  Checa novas mensagens
  if (raBot.getNewMessage(msg))
  {
    //  Verifica se nova mensagem é, ignorando capitalização, "Acender LED de alerta"
    if (msg.text.equalsIgnoreCase("Acender LED de alerta"))
    {
      //  Envia mensagem caso o LED já esteja aceso (flag ativada)
      if(isPirLEDOn)
      {
        raBot.sendMessage(msg.sender.id, "LED de alerta já aceso! Para apagá-lo, envie 'Apagar LED de alerta'");
      }
      //  Caso contrário, ativa flag e envia mensagem
      else
      {
        isPirLEDOn = true;
        raBot.sendMessage(msg.sender.id, "LED de alerta aceso!");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Apagar LED de alerta"
    else if (msg.text.equalsIgnoreCase("Apagar LED de alerta"))
    {
      //  Desativa flag do LED e envia mensagem, caso LED esteja aceso (flag ativada)
      if(isPirLEDOn)
      {
        isPirLEDOn = false;
        raBot.sendMessage(msg.sender.id, "LED de alerta apagado!");
      }
      //  Caso contrário, envia mensagem 
      else
      {
        raBot.sendMessage(msg.sender.id, "LED de alerta já apagado! Para acendê-lo, envie 'Acender LED de alerta'");
      }    
    }

    //  Envia resposta genérica para qualquer outra mensagem
    else
    {
      raBot.sendMessage(msg.sender.id, hello);
    }
  }
  
  //  Espera 0.1 segundos
  delay(100);
}

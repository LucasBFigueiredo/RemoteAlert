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
bool isLoudSpeakerOn = false;
bool isMuted = false;

TBMessage msg; //  Mensagem do Telegram a ser recebida 

String hello = "Olá e bem vindo ao RemoteAlert!\nVocê será notificado sempre que detectarmos novos movimentos. Também serão acionados o LED de alerta e o alto falante do sistema durante a presença dos movimentos.\nPara acender o led de alerta, envie 'Acender LED de alerta'. Para apagá-lo, envie 'Apagar LED de alerta'.\nPara ligar o alarme sonoro do sistema, envie 'Ligar alarme sonoro'. Para desligá-lo, envie 'Desligar alarme sonoro'.\nPara ativar o modo silencioso do sistema, envie 'Ativar modo silencioso'; note que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado. Para desativar o modo, envie 'Desativar modo silencioso'."";

void setup()
{
  //  Inicializa monitor serial
  Serial.begin(9600);
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
  raBot.sendMessage(msg.sender.id, hello);
}

void loop()
{  
  //  Lê sensor PIR e atualiza variáveis
  previousPirValue = pirValue;
  pirValue = digitalRead(PIR);

  //  Imprime no motor serial os estados atuais do 
  Serial.println("\nera: ");
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

  //  Ativa flag do alarme sonoro e envia mensagem ao usuário caso novo movimento tenha sido detectado e o sistema não esteja no modo silencioso
  if (!isMuted && !isLoudSpeakerOn && pirValue == HIGH && previousPirValue == LOW)
  {
    isLoudSpeakerOn = true;
    raBot.sendMessage(msg.sender.id, "Alarme sonoro ligado!");

  }

  //  Desativa flag do alarme sonoro e envia mensagem ao usuário caso movimentos deixem de ser detectados e o sistema não esteja no modo silencioso
  else if (!isMuted && isLoudSpeakerOn && pirValue == LOW && previousPirValue == HIGH)
  {
    isLoudSpeakerOn = false;
    raBot.sendMessage(msg.sender.id, "Alarme sonoro desligado!");
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
    tone(loudSpeaker, 700, 100);
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

    // Verifica se nova mensagem é, ignorando capitalização, "Ligar alarme sonoro"
    else if (msg.text.equalsIgnoreCase("Ligar alarme sonoro"))
    {
      //  Envia mensagem ao usuário caso o sistema esteja em seu modo silencioso
      if(isMuted)
      {
        raBot.sendMessage(msg.sender.id, "RemoteAlert está em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Envia mensagem ao usuário caso o sistema não esteja no modo silencioso, mas o alarme sonoro já esteja ligado
      else if (isLoudSpeakerOn)
      {
        raBot.sendMessage(msg.sender.id, "Alarme sonoro já ligado! Para desligá-lo, envie 'Desligar alarme sonoro'.");

      }
      //  Caso contrário, liga o alarme sonoro e envia mensagem ao usuário
      else
      {
        isLoudSpeakerOn = true;
        raBot.sendMessage(msg.sender.id, "Alarme sonoro ligado!");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Desligar alarme sonoro"
    else if (msg.text.equalsIgnoreCase("Desligar alarme sonoro"))
    {
      //  Envia mensagem ao usuário caso o sistema esteja em seu modo silencioso
      if(isMuted)
      {
        raBot.sendMessage(msg.sender.id, "RemoteAlert está em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Desliga o alarme sonoro e envia mensagem ao usuário, caso o sistema não esteja em seu modo silencioso e o alarme sonoro esteja ligado
      else if (isLoudSpeakerOn)
      {
        isLoudSpeakerOn = false;
        raBot.sendMessage(msg.sender.id, "Alarme sonoro desligado!");
      }
      //  Envia mensagem ao usuário caso contrário
      else
      {
        raBot.sendMessage(msg.sender.id, "Alarme sonoro já desligado! Para ligá-lo, envie 'Ligar alarme sonoro'.");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Ativar modo silencioso"
    else if (msg.text.equalsIgnoreCase("Ativar modo silencioso"))
    {
      //  Envia mensagem ao usuário caso o sistema já esteja em seu modo silencioso
      if (isMuted) {
        raBot.sendMessage(msg.sender.id, "RemoteAlert já está em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Ativa modo silencioso do sistema e envia mensagem do usuário, caso contrário
      else {
        isMuted = true;
        raBot.sendMessage(msg.sender.id, "RemoteAlert está agora em seu modo silencioso! Para desativar o modo, envie 'Desativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
    }

    // Verifica se nova mensagem é, ignorando capitalização, "Desativar modo silencioso"
    else if (msg.text.equalsIgnoreCase("Desativar modo silencioso"))
    {
      //  Desativa modo silencioso do sistema e envia mensagem ao usuário, caso o sistema esteja em seu modo silencioso
      if (isMuted) { 
        isMuted = false;    
        raBot.sendMessage(msg.sender.id, "Modo silencioso desativado! Para ativar o modo, envie 'Ativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");
      }
      //  Envia mensagem ao usuário, caso contrário
      else {
        raBot.sendMessage(msg.sender.id, "RemoteAlert não está modo silencioso! Para ativar o modo, envie 'Ativar modo silencioso'.\nNote que você não deixará de ser notificado de novos movimentos, apenas desativará o alto falante tornando o sistema mais difícil de ser detectado.");

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

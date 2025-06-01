#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>
#include <Ultrasonic.h>

WiFiClient wifiClient;                        

String estado = "";
//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;        

#define ID_MQTT  "lixeira-IoT"            //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior.
#define TOPIC_PUBLISH "projeto-solaria"    //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaMQTT();     //Faz conexão com Broker MQTT
void enviaPacote();     //

// 5 TRIGGER 4 ECHO
Ultrasonic ultrasonic(5, 4);
int distance;
int cheia; 
int vazia;
bool verifica = true; 
unsigned long segundos;
unsigned long segundosFuturo;

#define SMTP_HOST "smtp.gmail.com" // SMTP host
#define SMTP_PORT 465 // SMTP port

SMTPSession smtp;

#define AUTOR_EMAIL "lixeirainteligente82@gmail.com"
#define AUTOR_SENHA "tkuf kany vdwa vdrm"
bool conectado = false;

bool enviaEmail_TXT(String nomeRemetente,
                    String emailRemetente,
                    String senhaRemetente,
                    String assunto,
                    String nomeDestinatario,
                    String emailDestinatario,
                    String messageTXT,
                    String stmpHost,
                    int stmpPort);


#define EEPROM_SIZE 512

ESP8266WebServer server(80);

struct Config {
  char ssid[32];
  char password[32];
  char referencia[64];
  char nome[32];
  char identificacao[64];
  char email[128];
};

Config config;
bool dadosCarregados = false;

void salvarConfiguracao() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Configurações salvas na EEPROM.");
}

void carregarConfiguracao() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  EEPROM.end();

  if (strlen(config.ssid) > 0 && strlen(config.ssid) < 32) {
    dadosCarregados = true;
    Serial.println("Dados carregados da EEPROM.");
  } else {
    Serial.println("Dados da EEPROM inválidos.");
  }
}

void resetarConfiguracao() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("EEPROM resetada.");
}

void iniciarModoCadastro() {
  WiFi.mode(WIFI_AP);
  bool apCriado = WiFi.softAP("CadastroESP8266", "12345678");

  if (apCriado) {
    Serial.println("Modo cadastro iniciado com sucesso.");
    Serial.print("IP do AP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Falha ao iniciar o AP.");
  }

  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
                    <!DOCTYPE html>
                    <html lang="pt-BR">
                    <head>
                      <meta charset="UTF-8">
                      <meta name="viewport" content="width=device-width, initial-scale=1.0">
                      <title>Cadastro IoT</title>
                      <style>
                        body {
                          font-family: Arial, sans-serif;
                          background-color: #f5f5f5;
                          padding: 20px;
                          margin: 0;
                          display: flex;
                          justify-content: center;
                          align-items: center;
                          min-height: 100vh;
                        }
                        .container {
                          background-color: #fff;
                          padding: 25px 30px;
                          border-radius: 10px;
                          box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                          width: 100%;
                          max-width: 400px;
                        }
                        h2 {
                          text-align: center;
                          color: #333;
                          margin-bottom: 20px;
                        }
                        input[type="text"], input[type="password"] {
                          width: 100%;
                          padding: 10px;
                          margin: 8px 0 16px 0;
                          border: 1px solid #ccc;
                          border-radius: 5px;
                          box-sizing: border-box;
                        }
                        input[type="submit"] {
                          width: 100%;
                          background-color: #28a745;
                          color: white;
                          padding: 12px;
                          border: none;
                          border-radius: 5px;
                          cursor: pointer;
                          font-size: 16px;
                        }
                        input[type="submit"]:hover {
                          background-color: #218838;
                        }
                        .reset-btn {
                          background-color: #dc3545;
                          margin-top: 10px;
                        }
                        .reset-btn:hover {
                          background-color: #c82333;
                        }
                      </style>
                    </head>
                    <body>
                      <div class="container">
                        <h2>Cadastro do Dispositivo IoT</h2>
                        <form action="/cadastro" method="POST">
                          <label>SSID:</label>
                          <input type="text" name="ssid" required>
                          <label>Senha:</label>
                          <input type="password" name="password" required>
                          <label>Ponto de Referência:</label>
                          <input type="text" name="referencia" required>
                          <label>Nome do Remetente:</label>
                          <input type="text" name="nome" required>
                          <label>Identificação:</label>
                          <input type="text" name="identificacao" required>
                          <label>Email do Destinatário:</label>
                          <input type="text" name="email" required>
                          <input type="submit" value="Salvar e Conectar">
                        </form>
                        <form action="/reset" method="POST">
                          <input type="submit" class="reset-btn" value="Resetar Configurações">
                        </form>
                      </div>
                    </body>
                    </html>
                    )rawliteral";
    server.send(200, "text/html", html);
  });

  server.on("/cadastro", HTTP_POST, []() {
    strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid));
    strncpy(config.password, server.arg("password").c_str(), sizeof(config.password));
    strncpy(config.referencia, server.arg("referencia").c_str(), sizeof(config.referencia));
    strncpy(config.nome, server.arg("nome").c_str(), sizeof(config.nome));
    strncpy(config.identificacao, server.arg("identificacao").c_str(), sizeof(config.identificacao));
    strncpy(config.email, server.arg("email").c_str(), sizeof(config.email));
    salvarConfiguracao();

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    Serial.println("Tentando conectar à nova rede Wi-Fi...");

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
      delay(500);
      Serial.print(".");
      tentativas++;
    }

    String response;
    if (WiFi.status() == WL_CONNECTED) {
      conectado = true;
      Serial.println("\nConectado com sucesso!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      response = "<html><body style='text-align: center; padding-top: 3vh; font-size: 30vh;'><h2>Conectado com sucesso!</h2><p>IP: " + WiFi.localIP().toString() + "</p></body></html>";
    } else {
      Serial.println("\nFalha ao conectar.");
      response = "<html><body style='text-align: center; padding-top: 3vh; font-size: 30vh;'><h2>Falha ao conectar.</h2><p>Verifique SSID e senha. Reinicie o dispositivo.</p></body></html>";
    }

    server.send(200, "text/html", response);
  });

  server.on("/reset", HTTP_POST, []() {
    resetarConfiguracao();
    String response = "<html><body style='text-align: center; padding-top: 3vh; font-size: 30vh;'><h2>Configuração Resetada!</h2><p>Reinicie o dispositivo.</p></body></html>";
    server.send(200, "text/html", response);
  });

  server.begin();
  Serial.println("Servidor web iniciado no modo cadastro.");
}

void iniciarServidorNormal() {
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Cadastro Dispositivo</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background-color: #f5f5f5;
          margin: 0;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
        }
        .container {
          background-color: #fff;
          padding: 30px;
          border-radius: 10px;
          box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
          text-align: left;
          max-width: 400px;
          width: 90%;
        }
        h2 {
          color: #28a745;
          margin-bottom: 20px;
          text-align: center;
        }
        p {
          color: #333;
          font-size: 16px;
          margin: 8px 0;
        }
        a.reset-link {
          display: inline-block;
          margin-top: 20px;
          color: #dc3545;
          font-weight: bold;
          text-decoration: none;
        }
        a.reset-link:hover {
          text-decoration: underline;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Dispositivo conectado com sucesso!</h2>
        <p>Referência: )rawliteral" + String(config.referencia) + R"rawliteral(</p>
        <p>Nome: )rawliteral" + String(config.nome) + R"rawliteral(</p>
        <p>Identificação: )rawliteral" + String(config.identificacao) + R"rawliteral(</p>
        <p>Email: )rawliteral" + String(config.email) + R"rawliteral(</p>
        <p><a class="reset-link" href="/reset">Resetar Configuração</a></p>
      </div>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
  });

  server.on("/reset", HTTP_GET, []() {
    String html = R"rawliteral(
                    <!DOCTYPE html>
                    <html lang="pt-BR">
                    <head>
                      <meta charset="UTF-8">
                      <meta name="viewport" content="width=device-width, initial-scale=1.0">
                      <title>Resetar Configurações</title>
                      <style>
                        body {
                          font-family: Arial, sans-serif;
                          background-color: #f5f5f5;
                          display: flex;
                          justify-content: center;
                          align-items: center;
                          height: 100vh;
                          margin: 0;
                        }
                        .container {
                          background-color: #fff;
                          padding: 30px 25px;
                          border-radius: 10px;
                          box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                          max-width: 400px;
                          width: 90%;
                          text-align: center;
                        }
                        h2 {
                          color: #333;
                          margin-bottom: 20px;
                        }
                        input[type="submit"] {
                          background-color: #dc3545;
                          color: white;
                          padding: 12px 20px;
                          border: none;
                          border-radius: 5px;
                          font-size: 16px;
                          cursor: pointer;
                          width: 100%;
                        }
                        input[type="submit"]:hover {
                          background-color: #c82333;
                        }
                      </style>
                    </head>
                    <body>
                      <div class="container">
                        <h2>Tem certeza que deseja resetar?</h2>
                        <form action="/reset" method="POST">
                          <input type="submit" value="Confirmar Reset">
                        </form>
                      </div>
                    </body>
                    </html>
                    )rawliteral";
    server.send(200, "text/html", html);
  });

  server.on("/reset", HTTP_POST, []() {
    resetarConfiguracao();
    String response = "<html><body style='text-align: center; padding-top: 3vh; font-size: 30vh;'><h2>Configuração Resetada!</h2><p>Reinicie o dispositivo.</p></body></html>";
    server.send(200, "text/html", response);
  });

  server.begin();
  Serial.println("Servidor iniciado no modo normal.");
}

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  Serial.print("Conectando ao Wi-Fi");

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    conectado = true;
    Serial.println("\nConectado ao Wi-Fi!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar. Iniciando modo cadastro...");
    iniciarModoCadastro();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nInicializando...");
  carregarConfiguracao();

  if (dadosCarregados) {
    conectarWiFi();
    iniciarServidorNormal(); // IMPORTANTE
  } else {
    iniciarModoCadastro();
  }


  if (conectado) {
    smtp.debug(0);
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  }
}

void loop() {
  server.handleClient();
  if (conectado){mantemConexoes(); MQTT.loop();}

  unsigned long tempo = millis();
  segundos = int(tempo/1000);
  distance = ultrasonic.read();
  
  // encher lixeira
  if (distance <= 7 and cheia < 5 and segundos != segundosFuturo){cheia++;}
  else if (distance > 7){cheia = 0;}

  // esvaziar lixeira
  if (distance >= 20 and vazia < 8 and segundos != segundosFuturo){vazia++;}

  if (cheia == 5 and verifica == true and conectado)
  {
    MQTT.publish(TOPIC_PUBLISH, "1");
    Serial.println("Preparando envio de e-mail...");
    String email = "" + String(config.email);

    String messageTXT = "Prezado(a) Sr.(a). " + String(config.nome) + ",\n\n" +
                    "Este é um aviso para informar que a lixeira localizada no ponto de referência " + String(config.referencia) +
                    ", com o código " + String(config.identificacao) + ", encontra-se cheia e necessita de recolhimento.\n\n" +
                    "Agradecemos a sua atenção para que o esvaziamento seja providenciado o mais breve possível.";
    enviaEmail_TXT("Lixeira Inteligente",
                  AUTOR_EMAIL,
                  AUTOR_SENHA,
                  "Lixeira Cheia - Necessidade de Recolhimento",
                  "Lixeira Inteligente",
                  email,
                  messageTXT,
                  SMTP_HOST,
                  SMTP_PORT);
    Serial.println("Email enviado");
    verifica = false;
    cheia = 0;
    vazia = 0;
    delay(1000);
  }

  if (vazia == 5 and verifica == false)
  {
    MQTT.publish(TOPIC_PUBLISH, "0");
    verifica = true;
    cheia = 0;
    vazia = 0;
  }

  segundosFuturo = segundos;
}

bool enviaEmail_TXT(String nomeRemetente,
                    String emailRemetente,
                    String senhaRemetente,
                    String assunto,
                    String nomeDestinatario,
                    String emailDestinatario,
                    String messageTXT,
                    String stmpHost,
                    int stmpPort) {
                      
  // Objeto para declarar os dados de configuração da sessão
  ESP_Mail_Session session;
  // Defina os dados de configuração da sessão
  session.server.host_name = stmpHost;
  session.server.port = stmpPort;
  session.login.email = emailRemetente;
  session.login.password = senhaRemetente;
  session.login.user_domain = "";
  // Defina o tempo de configuração do NTP

  // Utilizado o NTP do Google: https://developers.google.com/time
  session.time.ntp_server = F("time.google.com");

  // define o deslocamento em segundos do fuso horário local em relação ao GMT do Meridiano de Greenwich.
  session.time.gmt_offset = -3; 
  
  // define o deslocamento em segundos do fuso horário local.
  session.time.day_light_offset = 0; 

  // Instanciação do objeto da classe de mensagem
  SMTP_Message message;
  // Definição os cabeçalhos das mensagens
  message.sender.name = nomeRemetente;
  message.sender.email = emailRemetente;
  message.subject = assunto;
  message.addRecipient(nomeDestinatario, emailDestinatario);
  message.text.content = messageTXT.c_str();
  // O conjunto de caracteres de mensagem de texto html, por exemplo:

  message.text.charSet = "utf-8";
  // A codificação de transferência de conteúdo. Ex:

  //  O valor padrão é "7bit"
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  // A prioridade da mensagem:

  //  O valor padrão é esp_mail_smtp_priority_low
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  // As notificações de status de entrega, Ex:

  message.response.notify = esp_mail_smtp_notify_success |
                            esp_mail_smtp_notify_failure |
                            esp_mail_smtp_notify_delay;
  // Conecte-se ao servidor com a configuração da sessão
  if (!smtp.connect(&session))
    return false;
  // Começa a enviar e-mail e fecha a sessão
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Erro ao enviar e-mail, " + smtp.errorReason());
    return false;
  }
  return true;
}

void mantemConexoes() {
    if (!MQTT.connected()) {
        conectaMQTT();
    }
}

void conectaMQTT() {
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao Broker com sucesso!");
        }
        else {
            Serial.println("Nao foi possivel se conectar ao broker.");
            Serial.println("Nova tentativa de conexao em 10s");
            delay(10000);
          }
      }
}
//inclusion des librairies
#include <WiFi.h>
#include <esp_sleep.h>
#include "FS.h" 
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"     //contrôle et de configuration des broches d'entrée/sortie (GPIO)
#include <EEPROM.h>            // read and write from flash memory
// definition d'un byte d'espace memoire dans L'EEPROM
#define EEPROM_SIZE 1
 


// Definitions des broches GPIO afin de connecter la caméra à l'esp32 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       210
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
 
int photoNumber = 0;//inclusion de la variable qui nous permettra de compter le nombre de photo prises permettant de reprendre le décompte
//definition des identifiants du réseau (ssid/ mot de passe)
const char* ssid = "nom du réseau";//Service set identifier
const char* password = "mot de passe";
  
void setup() {

  pinMode(4, INPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);//désactive la fonction de maintien de l'état de la broche GPIO 4 lors de la mise en veille profonde
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //desactive le detecteur de sous tension
  Serial.begin(115200);//vitesse de com entre l'esp32 et le pc
 
  Serial.setDebugOutput(true);//active la sortie de debogage de l'esp32 (si true alors  afficher sur le port série)
 
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  
    pinMode(4, INPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA| (taille de la photo ici 1600/1200)
        config.fb_count = 2;//Cela configure le nombre de tampons d'image (framebuffers) à 2. Les tampons d'image sont utilisés pour stocker les images capturées par la caméra.
  } else {                              //si mémoire psram pas détécté
    config.frame_size = FRAMESIZE_SVGA;//800*600px
    config.jpeg_quality = 12;//+ valeur grande + quelatité mais + fichier lourd
    config.fb_count = 1;//nombre de tampons di'mages à 1
  }
  // Initialisation de la caméra
  esp_err_t err = esp_camera_init(&config); // Cette ligne de code initialise la caméra en utilisant les paramètres de configuration définis dans la structure config. 
  if (err != ESP_OK) { //si initialisation échoué
    Serial.printf("Camera init failed with error 0x%x", err);// Cela affiche un message d'erreur indiquant que l'initialisation de la caméra a échoué
    return; //termine l'execution de la fonction setup
  }

  // Connexion Wi-Fi
WiFi.begin(ssid, password);//fonction de la bibliothèque WiFi.h qui permet de démarrer le processus de connexion Wi-Fi. (Wifi.Begin)
while (WiFi.status() != WL_CONNECTED) {//verifie si la connection wifi est etablis tant que ce n'est pas le cas Connecting to wifi
  delay(1000);
  Serial.println("Connexin au réseau wifi en cours...");
}

Serial.println("La connection au réseau WiFi à été établie avec succès");//s'execute lorsque la connection est établie
 
 //initialisation de la carte SD
  Serial.println("Démarage de la carte SD");
 
  delay(500);//laisse le temp à la carte sd de se connecté (0.5s)
  if (!SD_MMC.begin()) {//verifie que la carte sd à été corrêctement monté
    Serial.println("Échec de montage de la carte SD");//erreur si la carte sd n'est pas bien monté
    return;//met fin au programme
  }
 
  uint8_t cardType = SD_MMC.cardType();//verification du type de carte sd (SD, SDHC, MMC)
  if (cardType == CARD_NONE) {//vérifie si le type de carte détecté est égal à CARD_NONE. Cette valeur indique qu'aucune carte SD n'est attachée ou que la communication avec la carte a échoué.
    Serial.println("Aucune carte SD détécté");//aucune carte SD n'est présente (Cela indique que le programme n'a pas pu monter la carte SD avec succès, ce qui peut être dû à différentes raisons, telles que l'absence de carte SD, une carte SD défectueuse ou mal insérée, un problème matériel ou un format de carte incompatible.)
    return;//met fin au programme
  }
   
  camera_fb_t *fb = NULL; //crée une référence/un pointeur vers l'emplacement mémoire où les données de l'image sont stockées
 
  //Prendre une photo avec la caméra
  fb = esp_camera_fb_get(); // est appelée pour récupérer une image à partir de la caméra. Cette fonction renvoie un pointeur vers une structure camera_fb_t qui contient les données de l'image capturée. 
  if (!fb) {//berifie si le pointeur fb est valide,s'il pointe vers une image capturée avec succès.
    Serial.println("Camera capture failed");//message d'erreur
    return;//fin du programme
  }
 
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);//, initialise la mémoire EEPROM avec une taille prédéfinie. La constante EEPROM_SIZE indique la quantité de mémoire en bytes que nous souhaitons allouer pour la sauvegarde des données.
  photoNumber = EEPROM.read(0) + 1;//lit la valeur stockée à l'adresse mémoire 0 de l'EEPROM et l'assigne à la variable photoNumber. On ajoute 1 à cette valeur pour obtenir le numéro de la prochaine photo. Cela permet d'assurer que chaque nouvelle photo a un numéro unique.
  if (photoNumber > 10) {// qui vérifie si le numéro de la photo dépasse 10. Si c'est le cas, cela signifie que nous avons atteint le maximum de photos que nous souhaitons prendre
    photoNumber = 1; // redemarrer le comptage de 1 à 10 (sinombre de phto>10 alors 
  }
 
  //Chemin où sous sauvegardé les photo sur la carte SD
  String path = "/photo" + String(photoNumber) + ".jpg";
 
  fs::FS &fs = SD_MMC;//crée une sorte de "raccourci" ou de "lien" entre le système de fichiers utilisé (SD_MMC) et une variable appelée "fs". Cela signifie que lorsque nous utilisons cette variable "fs", nous pouvons accéder facilement aux fonctions et méthodes du système de fichiers sans avoir à écrire "SD_MMC" à chaque fois.
  Serial.printf("Nom du ficher photo: %s\n", path.c_str());//communique à l'utilisateur le nom du fichier
 
  File file = fs.open(path.c_str(), FILE_WRITE); // Cette ligne ouvre un fichier spécifié par le chemin path en mode écriture. La variable file représente le fichier ouvert.
  if (!file) {//Cette structure conditionnelle vérifie si l'ouverture du fichier a réussi ou non
    Serial.println("Impossible d'ouvrir le fichier en mode écriture");//code d'erreur
  } else {//sinon
    file.write(fb->buf, fb->len);//cette instruction écrit les données de l'image capturée (stockées dans le tampon fb->buf)
    Serial.printf("Saved file to path: %s\n", path.c_str());//chemin d'accès vers le fichier
    EEPROM.write(0, photoNumber);//enregistre la valeur photonumber dans l'EEPROM à l'adresse 0
    EEPROM.commit();//confirme l'enregistrement de la valeur dans l'eeprom
  }

  file.close();//ferme le fichier qui a été ouvert précédemment.
  esp_camera_fb_return(fb);// retourne le tampon de la caméra ('fb') à la mémoire de la caméra. Cela libère la mémoire utilisée par le tampon de l'image capturée
  
  delay(1000);//delaie avant de mettre en veille la caméra
  
  // Etteint le FLASH
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_en(GPIO_NUM_4);//active le mécanisme de verrouillage (hold) pour le GPIO 4, ce qui maintient son état même en mode veille.

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0);//configure le GPIO 13 (broche 13) comme une source d'interruption externe pour réveiller l'ESP32-CAM du mode veille. Lorsque le GPIO 13 reçoit un signal, l'ESP32-CAM sortira du mode veille.

  Serial.println("Mode veille activé");//mettre l'esp32 en veille
  delay(1000);
  esp_deep_sleep_start();//met l'esp32 en mode veille profonde
}
 
void loop() {
 
}

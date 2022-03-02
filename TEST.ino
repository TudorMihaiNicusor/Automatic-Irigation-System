#include <TimerOne.h>

#define DEBUG true

int IN1 = 12; //pinii pentru releu
int IN2 = 13;

int Pin1 = A0; //pinii pentru senzori
int Pin2 = A1;
int Pin3 = A2;
int Pin4 = 8; //pin pentru buzzer

float senzor1Value = 0; //umiditate1
float senzor2Value = 0; //umiditate2
float senzor3Value = 0; //nivel apa  

float h1p = 0; //humidity from sensor1 percentage
float h2p = 0; //humidity from sensor2 percentage
float wp = 0; //water depth

String force_pomp1 = "0"; //comenzi de la telefon
String force_pomp2 = "0";
String force_buzzer = "0";

String to_phone; //de vizualizat date telefon
char c_from_phone; //date primite de la telefon
String input_from_phone;

bool the_first_read = false; //prima cititre a senzorilor de umiditate
bool data_available = true; //astept dupa toate comenzile de la telefon

bool watered1 = false; //daca deja au fost activitate pompele pentru intervalul curent de citire a senzorilor
bool watered2 = false;

volatile int time_to_read_moisture = 0; //intervalul de timp pentru citirea umiditatilor = 1 ora 
volatile int time_to_func_pomp1 = 0; //intervalul de timp pentru functionarea primei pompe = 7 sec
volatile int time_to_func_pomp2 = 0; //intervalul de timp pentru functionarea pompei 2 = 10 sec
volatile int time_to_func_buzzer = 0; //intervalul de timp pentru functionarea buzzerului= 2 sec
volatile int time_to_wait_next_beep = 0; //intervalul de timp pana la urmatoarea functionarea a buzzerului = 30 sec

volatile bool moisture_read = false; //citit/necitit umiditate
volatile bool pomp1_func = false; //pornita/nepornita pompa1
volatile bool pomp1_working = false; //afla/nu se afla in procesul de functionare pompa1
volatile bool pomp2_func = false; //pornita/nepornita pompa2
volatile bool pomp2_working = false; //afla/nu se afla in procesul de functionare pompa2
volatile bool buzzer_func = false; //pornit/nepornit buzzer
volatile bool buzzer_waiting = false; //afla/nu se afla in procesul de asteptare 

void setup() {
  Serial.begin(9600);
  
  pinMode(Pin1, INPUT); //declar senzorii ca intrari
  pinMode(Pin2, INPUT);
  pinMode(Pin3, INPUT_PULLUP);

  pinMode(IN1, OUTPUT); //declar pompele si buzzerul ca iesiri
  pinMode(IN2, OUTPUT);
  pinMode(Pin4, OUTPUT);
  
  digitalWrite(IN1, HIGH); //inchid pompele si buzzerul
  digitalWrite(IN2, HIGH);
  digitalWrite(Pin4, HIGH);
  
  Timer1.initialize(1000000); //o data la 1 sec se apeleaza functia de intrerupere
  Timer1.attachInterrupt(increment_time);
}

void loop() {
  if (Serial.available()){ //semnalele de telefon sunt prioritare
      process_data();
  }
  
  if (!the_first_read){ //asiguram prima citire a senzorilor de umiditate
    the_first_read = true;
    read_moisture();
    moisture_read = false;
  }

  if (time_to_read_moisture == 35 && !moisture_read){ //urmatoarea citire a senzorilor de umiditate
    read_moisture();
  }
  
  read_water_level(); //citirea senzorului de nivel
  h1p = map(senzor1Value, 239, 459, 100, 0);
  h2p = map(senzor2Value, 239, 459, 100, 0);
  wp = map(senzor3Value, 70, 500, 0, 100);
  
  to_phone = (String) h1p + "%," + (String) h2p + "%," + (String) senzor3Value + "%"; //alegem datele ce vrem sa le trimitem pe telefon
  
  if (data_available){ //daca nu s-au citit toate datele, nu umblam la nimic
    if (senzor3Value <= 100){ //daca nu avem apa, nu avem ce face cu pompele
      sound_the_alarm();
    }
    else if (buzzer_func){ //daca intre timp s-a asigurat apa, putem continua
      buzzer_func = false;
      buzzer_waiting = false;
      time_to_wait_next_beep = 0;
      time_to_func_buzzer = 0;
      digitalWrite(Pin4, HIGH);
    }
    else //am apa in rezervor => ma pot ocupa de pompe
    {
      bring_the_flood(IN1, senzor1Value, watered1, force_pomp1, pomp1_func, pomp1_working);
      bring_the_flood(IN2, senzor2Value, watered2, force_pomp2, pomp2_func, pomp2_working);
    }
  }
}

//functie de citire si prelucrare a datelor venite prin bluetooth
void process_data(){
  c_from_phone = Serial.read();
  input_from_phone = input_from_phone + c_from_phone;
  
  if (c_from_phone == '*'){ //caracterul care delimiteaza un mesaj
    force_pomp1 = input_from_phone.substring(0, 1);
    force_pomp2 = input_from_phone.substring(1, 2);
    force_buzzer = input_from_phone.substring(2, 3);

    input_from_phone = "";
    data_available = true; //datele au fost citite complet
  }
  else
    data_available = false; 
}

//functie de control al pompelor
void bring_the_flood(int pin, float senzorValue, bool &watered, String force_pomp, volatile bool &pomp_func, volatile bool &pomp_working){
  if (!pomp_func){ //daca nu functioneaza deja
    if (force_pomp == "0"){ //semnalele de la telefon sunt prioritare
      if (senzorValue > 459 && !watered) { //solul trebuie sa fie uscat si sa nu fi udat deja
        pomp_func = true; //le dau drumul
        pomp_working = false; //inca nu au inceput sa functionez
        watered = true; //marchez ca udat
      }
      else{
        watered = true;
      }
    }
    else if (force_pomp == "1"){
      pomp_func = true;
      pomp_working = false;
      watered = true;
    }
  } 
  else if (!pomp_working){ //daca sunt pornite, dar inca nu functioneaza, pornesc acum
      digitalWrite(pin, LOW);
      pomp_working = true; //marchez ca functionale
  }
}

//functie de control al buzzerului
void sound_the_alarm(){
  if (force_buzzer == "0"){ //functionarea normala
    if (!buzzer_func){
      digitalWrite(Pin4, LOW);
      buzzer_func = true;
    }
  }
  else if (force_buzzer == "1"){ //oprirea fortata a acestuia
    buzzer_func = false;
    buzzer_waiting = false;
    time_to_wait_next_beep = 0;
    time_to_func_buzzer = 0;
  }
  //daca a fost oprit fortat, pana nu se pune apa in rezervor si se pune force_buzzerul inapoi pe 0, nu va mai suna
}

//functie de citire a nivelului apei din rezervor
void read_water_level(){
  senzor3Value = analogRead(Pin3);
}

//functie de citire a umiditatii solului
void read_moisture(){
  senzor1Value = analogRead(Pin1);
  senzor2Value = analogRead(Pin2);
  moisture_read = true;
  watered1 = false;
  watered2 = false;
}

//functie de timp atasata Timerului
void increment_time(){
  Serial.println(to_phone); //trimit datele catre telefon

  //incrementez timpii de lucru cand semnalele de control imi zic, iar la finalul acestora restez si timpii si semnalele
  //humidity functions
  time_to_read_moisture++;
  
  if (time_to_read_moisture == 3600){
    time_to_read_moisture = 0;
    moisture_read = false;
  }

  //pomp1 funcitons
  if(pomp1_working)
    time_to_func_pomp1++;
  
  if (time_to_func_pomp1 == 7){
    time_to_func_pomp1 = 0;
    digitalWrite(IN1, HIGH);
    pomp1_func = false;
    pomp1_working = false;
  }

  //pomp2 functions
  if(pomp2_working)
    time_to_func_pomp2++;
    
  if (time_to_func_pomp2 == 10){
    time_to_func_pomp2 = 0;
    digitalWrite(IN2, HIGH);
    pomp2_func = false;
    pomp2_working = false;
  }

  //buzzer functions
  if (buzzer_func)
    time_to_func_buzzer++;
    
  if (time_to_func_buzzer == 2){
    time_to_func_buzzer = 0;
    digitalWrite(Pin4, HIGH);
    buzzer_waiting = true;
  }

  if (buzzer_waiting)
    time_to_wait_next_beep++;
  
  if (time_to_wait_next_beep == 30){
    time_to_wait_next_beep = 0;
    buzzer_func = false;
    buzzer_waiting = false;
  }
}

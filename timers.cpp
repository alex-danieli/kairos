#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <wiringPi.h>
#include <locale.h>
#include <iostream>
#include <pthread.h>

#include "rfid.h"
#include "bcm2835.h"
#include "config.h"

// For MySQL Connection
#include <mysql.h>

// using namespace std;

uint8_t HW_init(uint32_t spi_speed, uint8_t gpio);


#define DRAW_LEFT_ARROW 	0
#define DRAW_RIGHT_ARROW 	1
#define DRAW_TIME_DATE 		2


#define TRUE 1

/* Raspberry GPIO->HC-SR04 CONFIG */
#define TRIG1 0
#define ECHO1 1
#define TRIG2 2
#define ECHO2 3
#define WORK_DISTANCE 5.0f

struct sensore{
	float distanza;
	int numero;
	long lastIn;
	long lastOut;
	float lastMinDistance;
} sensore1,sensore2;

/* ------------------------------ */

int TIME_FONT_SIZE=164;
int DATE_FONT_SIZE=40;

int ARROW_FONT_SIZE=100;

SDL_Window* window;
SDL_Renderer* renderer;

SDL_Texture
			*imageTime,
			*imageDate,
			*entryText,
			*exitText,
			*imageArrowRight[6];

SDL_Rect
		zoneImageTime,
		zoneImageDate,
		zoneImageArrowRight;

pthread_t	thrFrecciaSX,
			thrFrecciaDX,
			thrSensors,
			pth1,
			pth2;

int wW,wH;



SDL_Color whiteColor = { 255, 255, 255, 255 };
SDL_Color redColor = { 255, 0, 0, 255 };
SDL_Color greenColor = { 0, 255, 0, 255 };
SDL_Color blueColor = { 0, 0, 255, 255 };

std::string	displayTime,
			displayDate;

char sn_str[23];

//Main loop flag
bool quit = false;

//Draw elements
bool renderLeftArrow=false;
bool renderRightArrow=false;
bool tagReaderEnabled=true;

//Event handler
SDL_Event e;

// Defining Mysql Database Constant Variables
#define SERVER		"192.168.1.64"
#define USER		"kairos"
#define PASSWORD	"kairos"
#define DATABASE	"KAIROS"



// In case of error, print the error code and close the application
void check_error_sdl(bool check, const char* message) {
	if (check) {
		std::cout << message << " " << SDL_GetError() << std::endl;
		SDL_Quit();
		exit(-1);
	}
}

// In case of error, print the error code and close the application
void check_error_sdl_img(bool check, const char* message) {
	if (check) {
		std::cout << message << " " << IMG_GetError() << std::endl;
		IMG_Quit();
		SDL_Quit();
		exit(-1);
	}
}

SDL_Texture* renderText(const std::string &message, const std::string &fontFile,
	SDL_Color color, int fontSize, SDL_Renderer *renderer)
{
	//Open the font
	TTF_Font *font = TTF_OpenFont(fontFile.c_str(), fontSize);
	if (font == nullptr){
		// logSDLError(std::cout, "TTF_OpenFont");
		return nullptr;
	}
	//We need to first render to a surface as that's what TTF_RenderText
	//returns, then load that surface into a texture
	SDL_Surface *surf = TTF_RenderText_Blended(font, message.c_str(), color);
	if (surf == nullptr){
		TTF_CloseFont(font);
		// logSDLError(std::cout, "TTF_RenderText");
		return nullptr;
	}
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
	if (texture == nullptr){
		// logSDLError(std::cout, "CreateTexture");
	}
	//Clean up the surface and font
	SDL_FreeSurface(surf);
	TTF_CloseFont(font);
	return texture;
}

// Update the interface date and time
void updateDateTime(){
	time_t now;
	tm *ltm;
	now = time(0);
	ltm = localtime(&now);
	/* UCFIRST */
	char buf [125];
	strftime (buf,125,"%a, %d %B %Y",ltm);
	// puts (buf);
	buf[0] = toupper(buf[0]);
	for(int i=0; buf[i]; i++){
		if(isspace(buf[i-1])){
			buf[i] = toupper(buf[i]);
		}
	}
	/* ------------------- */
	if(ltm->tm_min<10)
		displayTime=":0"+std::to_string(ltm->tm_min);
	else
		displayTime=":"+std::to_string(ltm->tm_min);

	if(ltm->tm_hour<10)
		displayTime="0"+std::to_string(ltm->tm_hour)+displayTime;
	else
		displayTime=""+std::to_string(ltm->tm_hour)+""+displayTime;

	displayDate=buf;
}

void SDL_setup(){

	// Initialize locale to Italian
	setlocale(LC_ALL,"it_IT.utf8");

	// Initialize SDL
	check_error_sdl(SDL_Init(SDL_INIT_VIDEO) != 0, "Unable to initialize SDL");

	// Create and initialize a 1280x1024 window
	window = SDL_CreateWindow("Test SDL 2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
										  1280, 1024, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL );
	check_error_sdl(window == nullptr, "Unable to create window");

	SDL_GetWindowSize(window,&wW,&wH);
	TIME_FONT_SIZE=wH/6;
	DATE_FONT_SIZE=wH/24;

	// Create and initialize a hardware accelerated renderer that will be refreshed in sync with your monitor (at approx. 60 Hz)
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	check_error_sdl(renderer == nullptr, "Unable to create a renderer");

	// Set the default renderer color to corn blue
	SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255);

	// Clear the window content (using the default renderer color)
	SDL_RenderClear(renderer);

	if (TTF_Init() != 0){
		SDL_Quit();
	}

	// Initialize SDL_img
	int flags=IMG_INIT_JPG | IMG_INIT_PNG;
	int initted = IMG_Init(flags);
	check_error_sdl_img((initted & flags) != flags, "Unable to initialize SDL_image");

	// Load Resources
	updateDateTime();

	imageTime = renderText(displayTime, "./roboto.ttf",	whiteColor, TIME_FONT_SIZE, renderer);
	imageDate = renderText(displayDate, "./roboto.ttf",	whiteColor, DATE_FONT_SIZE, renderer);
	SDL_QueryTexture(imageDate, NULL, NULL, &(zoneImageDate.w), &(zoneImageDate.h));
	SDL_QueryTexture(imageTime, NULL, NULL, &(zoneImageTime.w), &(zoneImageTime.h));
	zoneImageTime.x = (wW-zoneImageTime.w)/2; zoneImageTime.y = wH/2-TIME_FONT_SIZE;
	zoneImageDate.x = (wW-zoneImageDate.w)/2; zoneImageDate.y = wH/2+DATE_FONT_SIZE*0;

	for(int z=0;z<6;z++){
		imageArrowRight[z] = renderText("m", "./arrows.ttf",	whiteColor, ARROW_FONT_SIZE, renderer);
		SDL_SetTextureAlphaMod(imageArrowRight[z],0);
	}

	SDL_QueryTexture(imageArrowRight[0], NULL, NULL, &(zoneImageArrowRight.w), &(zoneImageArrowRight.h));
	zoneImageArrowRight.x=0; zoneImageArrowRight.y=((wH/2+DATE_FONT_SIZE+wH)/2)-ARROW_FONT_SIZE/2;

}

Uint32 drawLeftArrow(Uint32 interval,void *param){
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = DRAW_LEFT_ARROW;
	event.user.data1 = (void *)SDL_GetTicks();
	event.user.data2 = (void *)0;
	SDL_PushEvent(&event);
	return interval;
}

Uint32 drawRightArrow(Uint32 interval,void *param){
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = DRAW_RIGHT_ARROW;
	event.user.data1 = (void *)SDL_GetTicks();
	event.user.data2 = (void *)0;
	SDL_PushEvent(&event);
	return interval;
}

Uint32 drawTimeDate(Uint32 interval,void *param){
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = DRAW_TIME_DATE;
	event.user.data1 = (void *)SDL_GetTicks();
	event.user.data2 = (void *)0;
	SDL_PushEvent(&event);
	return interval;
}

void *animateLeftArrow(void *){
	int alpha_array[12]={255,0,0,0,0,0,0,0,0,0,0,0};
	for(int j=0;j<12;j++){
		for(int i=0;i<6;i++){
			SDL_SetTextureAlphaMod(imageArrowRight[i],alpha_array[i]);
			SDL_Delay(7);
		}
		for(int k=11;k>0;k--){
			alpha_array[k]=alpha_array[k-1];
		}
		alpha_array[0]=alpha_array[1]-43;
		if(alpha_array[0]<0) alpha_array[0] = 0;
	}
	renderLeftArrow=false;
	return NULL;
}

void *animateRightArrow(void *){
	int alpha_array[12]={255,0,0,0,0,0,0,0,0,0,0,0};
	for(int j=0;j<12;j++){
		for(int i=0;i<6;i++){
			SDL_SetTextureAlphaMod(imageArrowRight[i],alpha_array[i]);
			SDL_Delay(7);
		}

		for(int k=11;k>0;k--){
			alpha_array[k]=alpha_array[k-1];
		}

		alpha_array[0]=alpha_array[1]-43;
		if(alpha_array[0]<0) alpha_array[0] = 0;

	}
	renderRightArrow=false;
	return NULL;
}

/* SENSOR CONTROL SECTION */
void wiringPi_setup() {
	wiringPiSetup();

	pinMode(TRIG1, OUTPUT);
	pinMode(ECHO1, INPUT);
	pinMode(TRIG2, OUTPUT);
	pinMode(ECHO2, INPUT);

	//TRIG pin must start LOW
	digitalWrite(TRIG1, LOW);
	digitalWrite(TRIG2, LOW);
}



void *getCM1(void *) {
	long mcs,
		 startTime,
		 travelTime;

	while(TRUE){
		digitalWrite(TRIG1, HIGH);
		delayMicroseconds(10);
		digitalWrite(TRIG1, LOW);
		while((digitalRead(ECHO1) == LOW));
		startTime = micros();
		while((digitalRead(ECHO1)==HIGH));
		mcs=micros();
		travelTime = mcs - startTime;
		sensore1.distanza = travelTime / 58.0f;
		if(sensore1.lastMinDistance>sensore1.distanza) sensore1.lastMinDistance=sensore1.distanza;
		if(sensore1.distanza <= WORK_DISTANCE) sensore1.lastIn = mcs;
		delay(60);
	}
	return NULL;
}

void *getCM2(void *) {
	long mcs,
		 startTime,
		 travelTime;

	while(TRUE){
		digitalWrite(TRIG2, HIGH);
		delayMicroseconds(10);
		digitalWrite(TRIG2, LOW);
		while((digitalRead(ECHO2) == LOW));
		startTime = micros();
		while((digitalRead(ECHO2)==HIGH));
		mcs=micros();
		travelTime = mcs - startTime;
		sensore2.distanza = travelTime / 58.0f;
		if(sensore2.lastMinDistance>sensore2.distanza) sensore2.lastMinDistance=sensore2.distanza;
		if(sensore2.distanza <= WORK_DISTANCE) sensore2.lastIn = mcs;
		delay(60);
	}
	return NULL;
}

int getLeftRight(){
	if(sensore1.lastIn<=sensore2.lastIn)
		return 2;
	else
		return 1;
}

int save_record(int direction){
	MYSQL *connect;
    // MYSQL_RES *res_set;
	int result;
	
	connect = mysql_init(NULL);

	if (!connect){
		std::cout << "Mysql Initialization Failed";
		return 1;
	}

	connect = mysql_real_connect(connect, SERVER, USER, PASSWORD, DATABASE, 0,NULL,0);

	if (connect){
		std::cout << "Connection Succeeded\n";
	}else{
		std::cout << "Connection Failed\n";
	}
	
	result = mysql_query (connect, (std::string("INSERT INTO records(record_date_time, sn, type) VALUES (NOW(),'")+
									sn_str+
									std::string("'")+
									std::string(" ,")+
									std::to_string(direction)+
									std::string(")")
								   ).c_str()
							 );
							 
	if (result!=0){
		std::cout << "Mysql INSERT Failed\n";
	}
	
	mysql_close (connect);

	return 0;
}



void *readSensors(void *) {
	pthread_create(&pth1,NULL,getCM1,NULL);
	pthread_create(&pth2,NULL,getCM2,NULL);

	int	moved_sensors1=0,
		moved_sensors2=0;
	long mcs;
    
    
    
	while(TRUE){
		mcs=micros();
		if(sensore1.distanza<WORK_DISTANCE && sensore1.distanza>1.0){
			moved_sensors1=1;
		}
		if(sensore2.distanza<WORK_DISTANCE && sensore2.distanza>1.0){
			moved_sensors2=1;
		}
		if(sensore1.distanza>WORK_DISTANCE && sensore2.distanza>WORK_DISTANCE && (moved_sensors1==1 ||  moved_sensors2==1) &&  (mcs-sensore2.lastIn>300000) && (mcs-sensore1.lastIn>300000) ){
			std::cout << (mcs-sensore1.lastIn) << " vs " << (mcs-sensore2.lastIn) << "\n";
			moved_sensors1=0;
			moved_sensors2=0;
			// 1=ENTRA, 2=ESCE
			int azione=getLeftRight();
			if(azione==1){
				save_record(1);
				system("espeak -a200 -vmb/mb-it3+f3 -k5 -s150 \"Entrata\" 2>/dev/null &");
				tagReaderEnabled=true;
				pthread_cancel(thrSensors);
				pthread_cancel(pth1);
				pthread_cancel(pth2);
				moved_sensors1=0;
				moved_sensors2=0;
				pthread_create(&thrFrecciaDX,NULL,animateRightArrow,NULL);
				pthread_detach(thrFrecciaDX);
				renderRightArrow=true;
			}

			if(azione==2){
				save_record(2);
				system("espeak -a200 -vmb/mb-it3+f3 -k5 -s150 \"Uscita\" 2>/dev/null &");
				tagReaderEnabled=true;
				pthread_cancel(thrSensors);
				pthread_cancel(pth1);
				pthread_cancel(pth2);
				moved_sensors1=0;
				moved_sensors2=0;
				pthread_create(&thrFrecciaSX,NULL,animateLeftArrow,NULL);
				pthread_detach(thrFrecciaSX);
				renderLeftArrow=true;
			}
		}
		delay(10);
	}
	return NULL;

}



/* ---------------------- */

int main(int argc, char** argv) {
	// SDL_TimerID timer = 0;
	uint8_t SN[10];
	uint16_t CType=0;
	uint8_t SN_len=0;
	// char status;
	int tmp;
	char *p;

	bcm2835_init();
	SDL_setup();
	wiringPi_setup();
	
	if (HW_init(5000,7)) return 1; 
	
	// timer = SDL_AddTimer(1000,drawTimeDate,NULL);
	SDL_AddTimer(1000,drawTimeDate,NULL);
	
	InitRc522();

	while( !quit ){
		if(tagReaderEnabled){
			// status=find_tag(&CType);
			find_tag(&CType);
			usleep(200);
			if (select_tag_sn(SN,&SN_len)==TAG_OK){
				pthread_create(&thrSensors,NULL,readSensors,NULL);
				p=sn_str;
				*(p++)=' ';
				for (tmp=0;tmp<SN_len;tmp++) {
					sprintf(p,"%02x",SN[tmp]);
					p+=2;
				}
				tagReaderEnabled=false;
			}
		}
		//Handle events on queue
		while( SDL_PollEvent( &e ) != 0 ){
			//User requests quit
			if( e.type == SDL_QUIT ){
				quit = true;
			}
			if( e.type == SDL_USEREVENT ){
				switch(e.user.code){
					case DRAW_LEFT_ARROW  :	break;
					case DRAW_RIGHT_ARROW :	break;
					case DRAW_TIME_DATE   :	updateDateTime();
											SDL_DestroyTexture(imageDate);
											SDL_DestroyTexture(imageTime);
											imageDate = renderText(displayDate, "./roboto.ttf",	whiteColor, DATE_FONT_SIZE, renderer);
											imageTime = renderText(displayTime, "./roboto.ttf",	whiteColor, TIME_FONT_SIZE, renderer);
											SDL_QueryTexture(imageDate, NULL, NULL, &(zoneImageDate.w), &(zoneImageDate.h));
											SDL_QueryTexture(imageTime, NULL, NULL, &(zoneImageTime.w), &(zoneImageTime.h));
											zoneImageTime.x = (wW-zoneImageTime.w)/2; zoneImageTime.y = wH/2-TIME_FONT_SIZE;
											zoneImageDate.x = (wW-zoneImageDate.w)/2; zoneImageDate.y = wH/2+DATE_FONT_SIZE*0;
											break;
				}
			}

			if( e.type == SDL_KEYDOWN){
				switch( e.key.keysym.sym ){
					case SDLK_LEFT:
							pthread_create(&thrFrecciaSX,NULL,animateLeftArrow,NULL);
							pthread_detach(thrFrecciaSX);
							renderLeftArrow=true;
							break;
					case SDLK_RIGHT:
							pthread_create(&thrFrecciaDX,NULL,animateRightArrow,NULL);
							pthread_detach(thrFrecciaDX);
							renderRightArrow=true;
							break;
					case SDLK_UP:
							break;
					case SDLK_DOWN:
							break;
					case SDLK_ESCAPE:
							quit = true;
							break;
					default:
							break;
				}
			}
		}

		if(renderRightArrow)
			for(int xx=0;xx<6;xx++){
				zoneImageArrowRight.x=wW/2-zoneImageArrowRight.w*(3-xx);
				SDL_RenderCopy(renderer, imageArrowRight[xx], NULL, &zoneImageArrowRight);
			}

		if(renderLeftArrow)
			for(int xx=0;xx<6;xx++){
				zoneImageArrowRight.x=wW/2+zoneImageArrowRight.w*(2-xx);
				SDL_RenderCopyEx(renderer, imageArrowRight[xx], NULL, &zoneImageArrowRight,0,NULL,SDL_FLIP_HORIZONTAL);
			}

		SDL_RenderCopy(renderer, imageDate, NULL, &zoneImageDate);
		SDL_RenderCopy(renderer, imageTime, NULL, &zoneImageTime);
		SDL_RenderPresent( renderer );
		SDL_RenderClear( renderer );
	}

	// Clear the allocated resources
	IMG_Quit();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}


uint8_t HW_init(uint32_t spi_speed, uint8_t gpio) {
	uint16_t sp;

	sp=(uint16_t)(250000L/spi_speed);
	if (!bcm2835_init()) {
		syslog(LOG_DAEMON|LOG_ERR,"Can't init bcm2835!\n");
		return 1;
	}
	if (gpio<28) {
		bcm2835_gpio_fsel(gpio, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_write(gpio, LOW);
	}

	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
	bcm2835_spi_setClockDivider(sp); // The default
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default
	return 0;
}





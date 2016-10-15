//------------------ Snake 3d ----------------//
//				  Projetado por:			  //
//			Nicolas Abril e Caio Kakuno		  //
//--------------------------------------------//


#include "msp430g2553.h"

/* 	Instrucoes de uso das portas:
	P1: 		dleds
	P2.[0..2]: 	saida teclado matriz
	P2.[3..5]: 	entrada teclado matriz
	P2.6:		ClkLed
	P2.7:		Som
*/
/*
	Ordenacao do cubo de leds:						|	Eixo x = esquerda/direita
	Cada grupo tem 8 leds ou 2 linhas de 4 leds		|	Eixo y = cima/baixo
	Leitura dos grupos:								|	
	Com os cabos virados para si					|	0	1
	1 esquerda -> direita							|	2	3
	2 cima -> baixo									|	4	5
													|	6	7
*/
/*
	Leitura de um andar de leds (visto de cima)
	0	1	|	0	1
	2	3	|	2	3
	4	5	|	4	5
	6	7	|	6	7
	Onde cada numero corresponde a um pino de P1 (um grupo por vez)
*/
/*
	Leitura da matriz campo_de_jogo, fixada a primeira coordenada
	[0][0]	[0][1]	[0][2]	[0][3]
	[1][0]
	[2][0]
	[3][0]		
*/
// Declaracao de Macros
#define dleds P1OUT
//#define SetClkLed P2OUT|=BIT6; //0b01000000
//#define ResetClkLed P2OUT&=~BIT6; //0b10111111

#define TAM_CAMPO 4
#define CAMPO_VAZIO 0
#define CAMPO_COMIDA 64
#define MAX_CONTADOR_FRAME_JOGO 250	//Numeros de frames graficos para 1 frame de gameplay (define a velocidade do jogo)


// Declaracao das funcoes
void Setup (void);
#pragma vector=TIMER0_A1_VECTOR
__interrupt void frame();
//__interrupt void teclado();
void Seleciona_dleds(char sel_dados);
void Printa_dleds(char m, char n);
void FrameJogo(void);
void Menu(void);
void Jogo(void);
void Anda(void);
void AndaECome(void);
void BateParede(void);
char TecladoMatriz(void);
void GeraComida(void);


// Declaracao de variaveis
int contador_frame = 0;
char campo_de_jogo[TAM_CAMPO][TAM_CAMPO][TAM_CAMPO];
char sel_dados = 0;						  //Numero do grupo de dados do campo [0..7], ordenados vide comentario acima
char pos_cabeca[3];						  //Coordenada cabeca (x,y,z)
char pos_comida[3];						  //Coordenada comida (x,y,z)
char dv[3];						  		  //Vetor velocidade (x,y,z)
char tam_cobra;						      //Tamanho atual da cobra	  
char modo_de_jogo;						  //0: Parede = morte		|1: Parede = da a volta		|2: Parede = absorve cabeca
char flag_jogo = 0;
char flag_pause = 0;
unsigned int rng, rng_x=1, rng_y=1;
// -----------------Algoritmos------------------ //

void main( void )
{
	Setup();
	while(1)
	{
		Menu();
		Jogo();
	}
}

//Configuracoes iniciais
void Setup (void)
{
	char i, j, k;
	WDTCTL = WDTPW + WDTHOLD;       // Para timer do watchdog
	//Configuracoes de P1 (dleds)
	P1DIR |= 0xFF;             	    // Define P1 como saida
	P1SEL &= ~0xFF;                 // Define modo dos pinos (io)
	P1SEL2 &= ~0xFF;                // Idem
	P1OUT &= 0x00;                  // Poe 0 em todos os pinos de P1
	
	//Configuracoes de P2 (teclado, clk_led e som)
	P2DIR =  0xC7;//0b11000111	    // Define 3 pinos do teclado(0,1,2) como saida e 3 pinos(3,4,5) como entrada; Define som e clk_led como saida (6 e 7)
	//P2IE |= BIT3;                 // Habilita interrupcao para p2.3
	//P2IFG &= ~BIT3;               // Limpa flag de interrupcao
	//P2IES |= BIT3;                // Determina que a interrupcao e por borda de descida
	P2REN |= 0x38;//0b00111000;     // Ativa pullup/pulldown para entrada do teclado
	
	// Inicia o shift register de sel e espera seu tempo de inicializacao acabar
        P2OUT&=~BIT6;
	for (i=7; i>0; i--)				// Manda 7 uns para o shift reg (nivel definido pelo capacitor de Ativador Shift Reg)
	{
		P2OUT|=BIT6;//SetClkLed;
		__delay_cycles(5);
		P2OUT&=~BIT6;//ResetClkLed;
	}
	__delay_cycles(2000000);		// Espera dois segundos para Ativador Shift Reg descarregar
	P2OUT|=BIT6;//SetClkLed;                // Manda o 0 do shift reg
	__delay_cycles(5);
	P2OUT&=~BIT6;//ResetClkLed;
	
	//Limpa campo_de_jogo
	
	for(i=0;i<TAM_CAMPO;i++)
	{
		for(j=0;j<TAM_CAMPO;j++)
		{
			for(k=0;k<TAM_CAMPO;k++)
			{
				campo_de_jogo[i][j][k]=CAMPO_VAZIO;
			}
		}
	}
	
	
	//Configuracoes TIMER0_A
	TA0CTL |= 	TASSEL_2		  //Seleciona SMCLK como counter (1MHz)
				+ID_0			  //Divide o clock por 1
				+MC_1;			  //Conta pra cima
	TA0CCR0 = 4000;		    	  //Contagem limite do timer (4ms por frame)
	TA0CTL &= ~TAIFG;		  	  //Reseta o flag de interreupcao do timer
	__enable_interrupt();		  //Habilita intererupcoes 
}

void Menu()
{
	char flag_comeco = 0;
	char tecla_pressionada;
	campo_de_jogo[0][0][0]= 1;
	campo_de_jogo[0][0][1]= 0;
	campo_de_jogo[0][1][0]= 1;
	campo_de_jogo[0][1][1]= 0;
	campo_de_jogo[0][2][0]= 0;
	campo_de_jogo[0][2][1]= 1;
	campo_de_jogo[0][3][0]= 0;
	campo_de_jogo[0][3][1]= 1;
	while (flag_comeco == 0)
	{
		tecla_pressionada = TecladoMatriz();
		switch (tecla_pressionada)
		{
			case 8:
				flag_comeco = 1;
				break;
			case 0:
				modo_de_jogo = 0;
				break;
			case 1:
				modo_de_jogo = 1;
				break;
			case 2:
				modo_de_jogo = 2;
				break;
		}
	}
}

void Jogo()
{
	char i,j,k;
	char tecla_pressionada;
	flag_pause = 0;
	//LimpaCampo
	for(i=0;i<TAM_CAMPO;i++)
	{
		for(j=0;j<TAM_CAMPO;j++)
		{
			for(k=0;k<TAM_CAMPO;k++)
			{
				campo_de_jogo[i][j][k]=CAMPO_VAZIO;
			}
		}
	}
	//Gera cabeca
	pos_cabeca[0] = 0;
	pos_cabeca[1] = 2;
	pos_cabeca[2] = 1;
	//Inicializa uma velocidade
	dv[0] = 1;
	dv[1] = 0;
	dv[2] = 0;
	//Cria a primeira comida
	GeraComida();
	flag_jogo = 1;
	while(flag_jogo)
	{
		tecla_pressionada = TecladoMatriz();
		if(flag_pause == 0)
		{
			switch (tecla_pressionada)
			{
				case 0:	//Esquerda
					dv[0] = -1;
					dv[1] = 0;
					dv[2] = 0;
					break;
				case 1:	//Cima
					dv[0] = 0;
					dv[1] = -1;
					dv[2] = 0;
					break;
				case 2:	//Tras
					dv[0] = 0;
					dv[1] = 0;
					dv[2] = -1;
					break;
				case 3:	//Direita
					dv[0] = 1;
					dv[1] = 0;
					dv[2] = 0;
					break;
				case 4:	//Baixo
					dv[0] = 0;
					dv[1] = 1;
					dv[2] = 0;
					break;
				case 5:	//Frente
					dv[0] = 0;
					dv[1] = 0;
					dv[2] = 1;
					break;
				case 8:
					flag_pause = 1;
					__delay_cycles(500000);
					break;
			}	
		}
		else
		{
			switch (tecla_pressionada)
			{
				case 8: 
					flag_pause = 0;
					break;
			}
		}
	}
}

//Geracao aleatoria de comida
void GeraComida()
{
	
	char flag_comida=0;
	while(flag_comida==0)
	{
		// Gerador de numeros aleatorios xorshift 16bits
		rng = (rng_x^(rng_x<<5)); 
		rng_x=rng_y;
		rng_y=(rng_y^(rng_y>>1))^(rng^(rng>>3));
		
		if(campo_de_jogo[rng_y&0x03][(rng_y&0x0C)>>2][(rng_y&0x30)>>4] == 0)
		{
			campo_de_jogo[rng_y&0x03][(rng_y&0x0C)>>2][(rng_y&0x30)>>4] = 64;
			flag_comida = 1;
		}
	}
}


//**********************************************************//
//Funcoes a serem executadas por interrupcoes

// Operacoes a serem executadas a cada frame grafico

__interrupt void frame()
{
	if(sel_dados == 7)
		sel_dados = 0;
	else
		sel_dados += 1;
	Seleciona_dleds(sel_dados);
	//MandaPulsoClkLed
	P2OUT|=BIT6;//SetClkLed;
	__delay_cycles(5);
	P2OUT&=~BIT6;//ResetClkLed;
	if (flag_pause == 0)
		if (++contador_frame == MAX_CONTADOR_FRAME_JOGO)
		{
			contador_frame = 0;
			FrameJogo();
			rng = (rng_x^(rng_x<<5));	//Muda o numero aleatorio para ele depender to tempo 
			rng_x=rng_y;
			rng_y=(rng_y^(rng_y>>1))^(rng^(rng>>3));
		}
}

/*#pragma vector=PORT2_VECTOR          	// Relaciona a funcao com o vetor de interrupcao da porta 1
__interrupt void teclado(void)
{
   P1OUT |= BIT0;                      	// high
   __delay_cycles(4000);               	// Espera 4ms
   P1OUT &= ~BIT0;                     	// low
   P1IFG &= ~BIT3;                     	// P1.3 flag de interrupcao cleared
}*/

//Seleciona qual grupo de dados vao ser colocados na saida
//sel_dados = numero do grupo de dados/leds
void Seleciona_dleds(char sel_dados)
{
	P1OUT = 0;
	switch (sel_dados)
	{
		case 0:
				Printa_dleds(0,0);
				break;
		case 1:
				Printa_dleds(0,2);
				break;
		case 2:
				Printa_dleds(1,0);
				break;
		case 3:
				Printa_dleds(1,2);
				break;
		case 4:
				Printa_dleds(2,0);
				break;
		case 5:
				Printa_dleds(2,2);
				break;
		case 6:
				Printa_dleds(3,0);
				break;
		case 7:
				Printa_dleds(3,2);
				break;
	}
}

//Joga duas fileiras de dados na saida
//m = andar inicial
//n = coluna inicial
void Printa_dleds(char m, char n)
{
	if(campo_de_jogo[m][0][n] > 0)
		P1OUT |= 0x01;//0b00000001;
	if(campo_de_jogo[m][0][n+1] > 0)
		P1OUT |= 0x02;//0b00000010;
	if(campo_de_jogo[m][1][n] > 0)
		P1OUT |= 0x04;//0b00000100;
	if(campo_de_jogo[m][1][n+1] > 0)
		P1OUT |= 0x08;//0b00001000;
	if(campo_de_jogo[m][2][n] > 0)
		P1OUT |= 0x10;//0b00010000;
	if(campo_de_jogo[m][2][n+1] > 0)
		P1OUT |= 0x20;//0b00100000;
	if(campo_de_jogo[m][3][n] > 0)
		P1OUT |= 0x40;//0b01000000;
	if(campo_de_jogo[m][3][n+1] > 0)
		P1OUT |= 0x80;//0b10000000;
}

//Operacoes a serem executadas a cada frame de gameplay (1 frame de gameplay = MAX_CONTADOR_FRAME_JOGO frames graficos)
void FrameJogo(void)
{
		// Se chegou em uma parede
		if ((pos_cabeca[0]+dv[0] >= 4)||(pos_cabeca[0]+dv[0] < 0)||(pos_cabeca[1]+dv[1] >= 4)||(pos_cabeca[1]+dv[1] < 0)||(pos_cabeca[2]+dv[2] >= 4)||(pos_cabeca[2]+dv[2] < 0))
			BateParede();
		// Se chegou em uma comida
		else if ((pos_cabeca[0]+dv[0] == pos_comida[0])&&(pos_cabeca[1]+dv[1] == pos_comida[1])&&(pos_cabeca[2]+dv[2] == pos_comida[2]))
			AndaECome();
		// Se so anda normal
		else
			Anda();
}

//Operacoes a serem executadas quando o jogador bater na parede
void BateParede()
{
	flag_jogo = 0;
}
	
//Cobra come um bloco (anda so a cabeca)
void AndaECome()
{
	tam_cobra++;
	pos_cabeca[0] = pos_comida[0];
	pos_cabeca[1] = pos_comida[1];
	pos_cabeca[2] = pos_comida[2];
	campo_de_jogo[pos_cabeca[0]][pos_cabeca[1]][pos_cabeca[2]] = tam_cobra;
	GeraComida();
}

//Movimenta a cobra 1 casa
void Anda()
{
	char i,j,k;
	pos_cabeca[0] += dv[0];
	pos_cabeca[1] += dv[1];
	pos_cabeca[2] += dv[2];
	for (i=0; i<TAM_CAMPO; i++)
		for (j=0; j<TAM_CAMPO; j++)
			for (k=0; k<TAM_CAMPO; k++)
				if(campo_de_jogo[i][j][k]>0 && campo_de_jogo[i][j][k]<64)
					campo_de_jogo[i][j][k] -= 1;
	campo_de_jogo[pos_cabeca[0]][pos_cabeca[1]][pos_cabeca[2]] = tam_cobra;
}


//**************************************************************//
// Funcoes de teclado
// Retorna qual botao esta sendo apertado no teclado em matriz
char TecladoMatriz(void)
{
	//Procura na primeira fileira
	P2OUT |= 0x01;//0b00000001;
	__delay_cycles(10);
	if(P2OUT&0x08)//0b00001000)
	{
		P2OUT &= 0xFE;//0b11111110;
		return 0;
	}
	else if(P2OUT&0x10)//0b00010000)
	{
		P2OUT &= 0xFE;//0b11111110;
		return 1;
	}	
	else if(P2OUT&0x20)//0b00100000)
	{
		P2OUT &= 0xFE;//0b11111110;
		return 2;
	}
	P2OUT &= 0xFE;//0b11111110;
	
	//Procura na segunda fileira
	P2OUT |= 0x02;//0b00000010;
	__delay_cycles(10);
	if(P2OUT&0x08)//0b00001000)
	{
		P2OUT &= 0xFD;//0b11111101;
		return 3;
	}
	else if(P2OUT&0x10)//0b00010000)
	{
		P2OUT &= 0xFD;//0b11111101;
		return 4;
	}
	else if(P2OUT&0x20)//0b00100000)
	{
		P2OUT &= 0xFD;//0b11111101;
		return 5;
	}
	P2OUT &= 0xFD;//0b11111101;
	
	//Procura na terceira fileira
	P2OUT |= 0x04;//0b00000100;
	__delay_cycles(10);
	if(P2OUT&0x08)//0b00001000)
	{
		P2OUT &= 0xFB;//0b11111011;
		return 6;
	}
	else if(P2OUT&0x10)//0b00010000)
	{
		P2OUT &= 0xFB;//0b11111011;
		return 7;
	}
	else if(P2OUT&0x20)//0b00100000)
	{
		P2OUT &= 0xFB;//0b11111011;
		return 8;
	}
	P2OUT &= 0xFB;//0b11111011;
	
	//Se nenhum botao foi pressionado
	return -1;
}
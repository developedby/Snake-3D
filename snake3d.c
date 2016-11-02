//------------------ Snake 3d ----------------//
//             	  Projetado por:	      	  //
//	  		Nicolas Abril e Caio Kakuno	  	  //
//--------------------------------------------//


#include "msp430g2553.h"

/* 	Instrucoes de uso das portas:
	P1.[0..2]: 	teclado input
	P1.[3..5]:	teclado output
	P2.0        dleds
	P2.1        clk_dleds
	P2.3:		sel
	P2.4:		clk_sel
	P2.6(XIN):  ativador_som
	P2.7(XOUT): som
*/
/*
	Ordenacao do cubo de leds:					|	Eixo x = esquerda/direita
	Cada grupo tem 8 leds ou 2 linhas de 4 leds	|	Eixo y = cima/baixo
	Leitura dos grupos:							|	
	Com os cabos virados para si				|	0	1
	1 esquerda -> direita				        |	2	3
	2 cima -> baixo								|	4	5
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
/*
	Leitura do teclado
	.	1	2
	0	.	5
	3	4	.
	.	.	.
	6	7	8
	
	. indica espaco sem botao
	
	0 - Esquerda
	1 - Cima
	2 - Tras
	3 - Frente
	4 - Baixo
	5 - Direita
	6 - +
	7 - -
	8 - Start
*/
// Declaracao de Macros

//Define botoes do teclado e direcoes
#define esquerda 0
#define direita 5
#define frente 3
#define tras 2
#define cima 1
#define baixo 4
#define mais 6
#define menos 7
#define start 8
 
//Define pinos da porta 1 (teclado)
#define pin_kb_in_0 BIT0
#define pin_kb_in_1 BIT1
#define pin_kb_in_2 BIT2
#define pin_kb_out_0 BIT3
#define pin_kb_out_1 BIT4
#define pin_kb_out_2 BIT5

//Define pinos da porta 2
#define pin_dleds BIT0
#define pin_clk_dleds BIT1
#define pin_sel BIT3
#define pin_clk_sel BIT4
#define pin_som BIT6
#define pin_ativador_som BIT7

//Define valor de campo_de_jogo
#define TAM_CAMPO 4
#define CAMPO_VAZIO 0
#define CAMPO_COMIDA 64

//Define frame de gameplay
#define MAX_CONTADOR_FRAME_JOGO 250	//Numeros de frames graficos para 1 frame de gameplay (define a velocidade do jogo)

//Define os modos de jogo
#define PAREDE_MATA 1
#define PAREDE_ATRAVESSA 2
#define PAREDE_ENCOLHE 3


// Declaracao das funcoes
void Setup (void);
#pragma vector=TIMER0_A1_VECTOR
__interrupt void frame();
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
char ChecaEntradasTeclado(void);
void MudaModoDeJogo(char mudanca);
void EscreveCaracter(char caracter, char layer);
void EscreveModoDeJogo(char layer);

// Declaracao de variaveis
int contador_frame = 0;
char campo_de_jogo[TAM_CAMPO][TAM_CAMPO][TAM_CAMPO];
char sel_dados = 0;						  //Numero do grupo de dados do campo [0..7], ordenados vide comentario acima
char dleds[8];                            //Vetor contendo os dados para serem enviados para os leds
char pos_cabeca[3];						  //Coordenada cabeca (x,y,z)
char pos_comida[3];						  //Coordenada comida (x,y,z)
char dv[3];						  		  //Vetor velocidade (x,y,z)
char tam_cobra;						      //Tamanho atual da cobra	  
char modo_de_jogo;						  //0: Parede = morte		|1: Parede = da a volta		|2: Parede = absorve cabeca
char flag_jogo = 0;						  //Indica se esta no meio de um jogo 
char flag_pause = 0;					  //Indica se o jogo esta pausado
unsigned int rng, rng_x=1, rng_y=1;		  //Variaveis para geracao de numeros aleatorios


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
       
	//Define o modo_de_jogo inicial
	modo_de_jogo = PAREDE_MATA;
	   
	//Configuracoes de P1 (teclado)
	
	// Define modo dos pinos (io)
    P1SEL &= ~0xFF;
	P1SEL2 &= ~0xFF;           
	
	// Define quais pinos sao entrada e quaius sao saida
	P1DIR &= ~(pin_kb_in_0 + pin_kb_in_1 + pin_kb_in_2); 
	P1DIR |= pin_kb_out_0 + pin_kb_out_1 + pin_kb_out_2;
	
	// Ativa pullup/pulldown para entrada do teclado
    P1REN |= pin_kb_in_0 + pin_kb_in_1 + pin_kb_in_2;
	
	// Define dados de saida e pullup/down de entrada 
	P1OUT &= ~(pin_kb_in_0 + pin_kb_in_1 + pin_kb_in_2);   //Pullup nas entradas
	P1OUT &= ~(pin_kb_out_0 + pin_kb_out_1 + pin_kb_out_2) //Inicia com 0
	
	//Configuracoes de P2 (leds e som)
    P2SEL &= ~0xFF;                 // Define modo dos pinos (io)
	P2SEL2 &= ~0xFF;
	
	P2DIR =  0xFF;          	    // Define todos os pinos de p2 como saida
    P2OUT = 0x00;					// Inicia as saidas com 0
	
	
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
	
	//Prepara o som
	P2OUT |= pin_ativador_som;
	P2OUT &= ~(pin_ativador_som);
	P2OUT |= pin_ativador_som;
	
	//Configuracoes TIMER0_A
	TA0CTL |= 	TASSEL_2		  //Seleciona SMCLK como counter (1MHz)
				+ID_0			  //Divide o clock por 1
				+MC_1;			  //Conta pra cima
	TA0CCR0 = 4000;		    	  //Contagem limite do timer (4ms por frame)
	TA0CTL &= ~TAIFG;		  	  //Reseta o flag de interreupcao do timer
	__enable_interrupt();		  //Habilita intererupcoes 
}


//Mostra o menu e aguarda instrucoes (mudar modo de jogo ou comecar)
void Menu()
{
    char i, j, k;
	char flag_comeco = 0;
	char tecla_pressionada;
	
	//Limpa o campo
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
	
	//Escreve modo de jogo inicial no andar 0;
	EscreveModoDeJogo(0);
	
	//Espera um botao
	while (flag_comeco == 0)
	{
		tecla_pressionada = TecladoMatriz();
		switch (tecla_pressionada)
		{
			case start:
				flag_comeco = 1;
				break;
			case mais:
				MudaModoDeJogo(1);
				break;
			case menos:
				MudaModoDeJogo(-1);
				break;
		}
	}
}
//Aumenta ou diminui o modo de jogo de forma ciclica, dependendo do sinal do parametro
void MudaModoDeJogo(char mudanca)
{
	if(mudanca>0)
		if(modo_de_jogo<2)
			modo_de_jogo++;
		else
			modo_de_jogo=0;
	else if(mudanca<0)
		if(modo_de_jogo>0)
			modo_de_jogo--;
		else
			modo_de_jogo=2;
	
}

//Escreve um caracter em campo_de_jogo, dado que o mesmo esteja limpo
void EscreveCaracter(char caracter, char layer)
{
	switch(caracter)
	{
		case '1':
			campo_de_jogo[layer][0][0] = 1;		//  O O . .
			campo_de_jogo[layer][0][1] = 1;		//	. O . .
			campo_de_jogo[layer][1][1] = 1;		//	. O . .
			campo_de_jogo[layer][2][1] = 1;		//	O O O .
			campo_de_jogo[layer][3][0] = 1;
			campo_de_jogo[layer][3][1] = 1;
			campo_de_jogo[layer][3][2] = 1;
			break;
		case '2':
			campo_de_jogo[layer][0][0] = 1;		//  O O . .
			campo_de_jogo[layer][0][1] = 1;		//	. . O .
			campo_de_jogo[layer][1][2] = 1;		//	. O . .
			campo_de_jogo[layer][2][1] = 1;		//	O O O .
			campo_de_jogo[layer][3][0] = 1;
			campo_de_jogo[layer][3][1] = 1;
			campo_de_jogo[layer][3][2] = 1;
			break;
		case '3':
			campo_de_jogo[layer][0][0] = 1;		//  O O O .
			campo_de_jogo[layer][0][1] = 1;		//	. O O .
			campo_de_jogo[layer][0][2] = 1;		//	. . O .
			campo_de_jogo[layer][1][1] = 1;		//	O O O .
			campo_de_jogo[layer][1][2] = 1;
			campo_de_jogo[layer][2][3] = 1;
			campo_de_jogo[layer][3][0] = 1;
			campo_de_jogo[layer][3][1] = 1;
			campo_de_jogo[layer][3][2] = 1;
			break;
	}
}

//Escreve o numero correspondente ao modo_de_jogo no andar layer de campo_de_jogo
void EscreveModoDeJogo(char layer)
{
	switch(modo_de_jogo)
	{
		case PAREDE_MATA:
			EscreveCaracter('1',layer);
			break;
		case PAREDE_ATRAVESSA:
			EscreveCaracter('2',layer);
			break;
		case PAREDE_ENCOLHE:
			EscreveCaracter('3',layer);
			break;
	}
}
//Funcao contendo a parte jogavel
//A cada frame de gameplay anda um quadrado na direcao do ultimo botao apertado
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
				case esquerda:
					dv[0] = -1;
					dv[1] = 0;
					dv[2] = 0;
					break;
				case cima:
					dv[0] = 0;
					dv[1] = -1;
					dv[2] = 0;
					break;
				case tras:
					dv[0] = 0;
					dv[1] = 0;
					dv[2] = -1;
					break;
				case direita
					dv[0] = 1;
					dv[1] = 0;
					dv[2] = 0;
					break;
				case baixo:
					dv[0] = 0;
					dv[1] = 1;
					dv[2] = 0;
					break;
				case frente
					dv[0] = 0;
					dv[1] = 0;
					dv[2] = 1;
					break;
				case start:
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
	//Acende o proximo grupo de leds
	if(sel_dados == 7)
		sel_dados = 0;
	else
		sel_dados += 1;
	Seleciona_dleds(sel_dados);
	
	//MandaPulsoClkLed
	P2OUT|=BIT6;//SetClkLed;
	__delay_cycles(5);
	P2OUT&=~BIT6;//ResetClkLed;
	
	//Se estiver jogando
	if (flag_pause == 0)
		//Incrementa o contador de frames e, se chegar a um frame de gameplay
		if (++contador_frame == MAX_CONTADOR_FRAME_JOGO)
		{
			//Reseta o contador de frames
			contador_frame = 0;
			//Executa um frame de gameplay
			FrameJogo();
			//Atualiza o numero aleatorio
			rng = (rng_x^(rng_x<<5));	//Muda o numero aleatorio para ele depender to tempo 
			rng_x=rng_y;
			rng_y=(rng_y^(rng_y>>1))^(rng^(rng>>3));
		}
}

//Seleciona qual grupo de dados vao ser colocados na saida
//sel_dados = numero do grupo de dados/leds
void Seleciona_dleds(char sel_dados)
{
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
	int i;
	//Define quais leds vao acender
	dleds[0] = campo_de_jogo[m][0][n] ? 1 : 0;
	dleds[1] = campo_de_jogo[m][0][n+1] ? 1 : 0;
	dleds[2] = campo_de_jogo[m][1][n] ? 1 : 0;
	dleds[3] = campo_de_jogo[m][1][n+1] ? 1 : 0;
	dleds[4] = campo_de_jogo[m][2][n] ? 1 : 0;
	dleds[5] = campo_de_jogo[m][2][n+1] ? 1 : 0;
	dleds[6] = campo_de_jogo[m][3][n] ? 1 : 0;
	dleds[7] = campo_de_jogo[m][3][n+1] ? 1 : 0;
	
	//Manda as informacoes para pin_dleds
	P2OUT &= ~pin_clk_dleds;
	for (i=0; i<8; i++)
	{
	  P2OUT &= ~pin_dleds;
	  if (dleds[i])
		P2OUT |= pin_dleds;
	  P2OUT |= pin_clk_dleds;
	  P2OUT &= ~pin_clk_dleds;
	}   
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
	switch(modo_de_jogo)
	{
		//Se modo de jogo e 1 (Parede mata), acaba o jogo
		case PAREDE_MATA:
			flag_jogo = 0;
			break;
		//Se modo de jogo e 2 (Parede atravessa), aparece com a cabeca do outro lado
		case PAREDE_ATRAVESSA:
			break;
		//Se modo de jogo e 3 (Parede encolhe), diminui em 1 o tamanhoda cobra sem se mover
		case PAREDE_ENCOLHE:
			break;
	}
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
	//Limpa a saida do teclado
	P1OUT &= ~(pin_kb_out_0 + pin_kb_out_1 + pin_kb_out_2);
	
	//Busca na fileira 0
	P1OUT |= pin_kb_out_0;
	if(ChecaEntradasTeclado()>0)
		return ChecaEntradasTeclado;
	P1OUT &= ~pin_kb_out_0;
	P1OUT |= pin_kb_in_1;
	if(ChecaEntradasTeclado()>0)
		return ChecaEntradasTeclado + 3;
	P1OUT &= ~pin_kb_out_1;
	P1OUT |= pin_kb_in_2;
	if(ChecaEntradasTeclado()>0)
		return ChecaEntradasTeclado + 6;
	P1OUT &= ~pin_kb_out_2;
	return -1;
}
//Verifica se um botao da fileira sendo checada esta apertado
char ChecaEntradasTeclado()
{
	if(P1OUT & pin_kb_in_0)
		return 0;
	if(P1OUT & pin_kb_in_1)
		return 1;
	if(P1OUT & pin_kb_in_2)
		return 2;
	return -1;
}
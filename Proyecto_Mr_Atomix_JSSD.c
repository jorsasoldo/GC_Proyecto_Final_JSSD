#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

//AMBAS LIBRERIAS SON MULTIPLATAFORMA
//yo las instale directamente al mingw64

//libreria de imagenes para el texturizado, originalmente iba a usar stb_image pero de ahi vi que existia SOIL que funciona encima de ella
//que lo adaptaba mejor pero despues me di cuenta que existia SOIL2 que es una version actualizada de SOIL original
//https://github.com/SpartanJ/SOIL2
#include <SOIL2.h> 

//libreria de audio, originalmente iba a incluir la libreria openAl pero esa libreria es mas para audio en 3d y mi animacion al ser principalmente 2d
//complicaba mucho las cosas
//https://github.com/mackron/miniaudio
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#define PI 3.14159265358979323846

//Arbol de la jerarquia de cada escena
typedef struct NodoJerarquia 
{
    int id_jerarquia;
    int tipo; //Bandera que indica que estructura tiene almacenada dentro del nodo de dato
    
    //Transformaciones locales
    double pos_x, pos_y, pos_z;
    double rot_x, rot_y, rot_z;
    double escala;
    
    void *dato; //Puntero void a personaje, objeto, recurso
    bool activo; //Bandera que indica si el nodo sera visible o no para evitar eliminarlo
    
    struct NodoJerarquia *padre;
    struct NodoJerarquia *hijo;
    struct NodoJerarquia *hermano;
}NodoJerarquia;

//Pila que indica el orden en el cual se renderizan los objetos en cada frame
typedef struct NodoPilaRenderizado 
{
    NodoJerarquia *nodo; //Referencia al nodo a renderizar
    struct NodoPilaRenderizado *sig;
}NodoPilaRenderizado;

typedef struct PilaRenderizado 
{
    NodoPilaRenderizado *tope;
}PilaRenderizado;

typedef struct Frame 
{
    int id_frame;
    double tiempo_duracion;
    NodoJerarquia *arbol_jerarquia;
    PilaRenderizado *pila_renderizado; 
    struct Frame *sig;
}Frame;


typedef struct Escena 
{
    int id_escena;
    char nombre[100];
    Frame *primer_frame;
    double duracion_total;
    struct Escena *sig;
}Escena;


typedef struct Pelicula 
{
    Escena *frente;
    Escena *final;
    int num_escenas;
}Pelicula;


typedef struct NodoPilaFrame 
{
    Frame *frame_actual;
    Escena *escena_actual;
    struct NodoPilaFrame *sig;
}NodoPilaFrame;

typedef struct PilaFrames 
{
    NodoPilaFrame *tope;
    int limite;
}PilaFrames;

typedef struct Textura 
{
    GLuint id_textura;
    char nombre[100];
    int ancho;
    int alto;
}Textura;

typedef struct Audio 
{
    ma_decoder decoder; //Decodificador de miniaudio
    char nombre[100];
    float duracion;
    bool cargado;
    bool reproduciendo;
}Audio;

typedef struct Dialogo 
{
    char texto[500];
    Audio *audio;
    float tiempo_mostrado;
    int activo;
}Dialogo;

typedef struct NodoRecurso 
{
    char ruta[256];
    int tipo;  //indica si es textura, sonido, etc
    void *dato_cargado;
    struct NodoRecurso *sig;
}NodoRecurso;

typedef struct ColaRecursos 
{
    NodoRecurso *frente;
    NodoRecurso *final;
}ColaRecursos;


typedef struct Punto
{
    int id;
    double x;
    double y;
    double z;

    //coordenadas de textura
    double u;
    double v;
}Punto;

typedef struct Personaje
 {
    int id;
    char nombre[50];
    
    Punto *punto_rotacion;
    Punto *puntos_figura;
    int num_puntos;
    
    double angulo_actual;
    double escala_x, escala_y;

    Textura *textura;
    Dialogo *dialogo;
    
    struct Personaje *padre;
    struct Personaje *hijo;
    struct Personaje *hermano;
}Personaje;


int main(int argc, char** argv)
{
    return 0;
}
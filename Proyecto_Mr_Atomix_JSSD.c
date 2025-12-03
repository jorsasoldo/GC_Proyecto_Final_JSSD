#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#define PI 3.14159265358979323846

//Arbol de la jerarquia de cada escena
typedef struct NodoJerarquia 
{
    char nombre[50];
    int tipo;  //Bandera que indica que estructura tiene almacenada dentro del nodo de dato
    
    //Transformaciones locales
    double pos_x, pos_y, pos_z;
    double rot_x, rot_y, rot_z;
    double escala;
    
    void *dato; //Puntero void a personaje, objeto, recurso
    int activo; //Bandera que indica si el nodo sera visible o no para evitar eliminarlo
    
    struct NodoJerarquia *padre;
    struct NodoJerarquia **hijos;
    int num_hijos;
    int capacidad_hijos;
}NodoJerarquia;

//Pila que indica el orden en el cual se renderizan los objetos en cada frame
typedef struct NodoPilaRenderizado 
{
    NodoJerarquia *nodo;  //Referencia al nodo a renderizar
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
    Frame *primer_frame;  //Lista enlazada de frames
    double duracion_total;
    struct Escena *sig;
}Escena;


typedef struct Pelicula 
{
    char titulo[100];
    Escena *frente;  //Primera escena
    Escena *final;   //Última escena
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


typedef struct NodoRecurso 
{
    char ruta[256];
    int tipo;  //indica si es textura, sonido, etc
    void *dato_cargado;
    struct NodoRecurso *sig;
} NodoRecurso;

typedef struct ColaRecursos 
{
    NodoRecurso *frente;
    NodoRecurso *final;
} ColaRecursos;

typedef struct Personaje
 {
    int id;
    char nombre[50];
    
    Punto *punto_rotacion;
    Punto **puntos_asociados;
    int num_puntos;
    
    double angulo_actual;
    double escala_x, escala_y;
    
    struct Personaje *padre;
    struct Personaje **hijos;
    int num_hijos;
    
    struct Personaje *sig;
} Personaje;

typedef struct Punto
{
    int id;
    double x;
    double y;
    double z;

}Punto;



int main(int argc, char** argv)
{
    return 0;
}
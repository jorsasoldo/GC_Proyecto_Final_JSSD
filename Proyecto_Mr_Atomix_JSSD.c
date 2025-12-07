//gcc Proyecto_Mr_Atomix_JSSD.c -o a.exe -lSOIL2 -lfreeglut -lopengl32 -lglu32

#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
    bool activo;
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

typedef struct Boton 
{
    float x, y;
    float ancho, alto;
    char texto[20];
    void (*accion)();
    bool hover;
}Boton;

ma_engine motor_audio;
bool audio_inicializado = false;
Pelicula *pelicula_global = NULL;
Escena *escena_actual = NULL;
Frame *frame_actual = NULL;
double tiempo_acumulado = 0.0;
int ultimo_tiempo = 0;
PilaFrames *pila_deshacer = NULL;
bool en_pausa = false;
int ventana_principal;
int ventana_controles;
Boton botones[4];
int num_botones = 4;

Punto *crea_punto(int id, double x, double y, double z, double u, double v) 
{
    Punto *p = (Punto*)malloc(sizeof(Punto));

    if(p == NULL)
        return NULL;

    p->id = id;
    p->x = x;
    p->y = y;
    p->z = z;
    p->u = u;
    p->v = v;
    return p;
}

void free_punto(Punto *p) 
{
    if(p != NULL) 
        free(p);
}

Textura *carga_textura(char *ruta) 
{
    Textura *tex = (Textura*)malloc(sizeof(Textura));

    if(tex == NULL)
        return NULL;
        
    strcpy(tex->nombre, ruta);
    
    tex->id_textura = SOIL_load_OGL_texture
    (
        ruta,
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y
    );
    
    if(tex->id_textura == 0) 
    {
        printf("Error: %s\n", SOIL_last_result());
        free(tex);
        return NULL;
    }
    
    glBindTexture(GL_TEXTURE_2D, tex->id_textura);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex->ancho);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex->alto);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return tex;
}

Textura *carga_textura_transparente(char *ruta) 
{
    Textura *tex = (Textura*)malloc(sizeof(Textura));
    strcpy(tex->nombre, ruta);
    
    tex->id_textura = SOIL_load_OGL_texture
    (
        ruta,
        SOIL_LOAD_RGBA,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y
    );
    
    if (tex->id_textura == 0) 
    {
        printf("Error al cargar textura transparente: %s\n", ruta);
        free(tex);
        return NULL;
    }
    
    glBindTexture(GL_TEXTURE_2D, tex->id_textura);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex->ancho);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex->alto);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return tex;
}

void free_textura(Textura *tex) 
{
    if(tex == NULL) 
        return;

    glDeleteTextures(1, &tex->id_textura);
    free(tex);
}

int inicializa_audio() 
{

    ma_result resultado = ma_engine_init(NULL, &motor_audio);

    if(resultado != MA_SUCCESS) 
    {
        printf("Error al inicializar miniaudio: %d\n", resultado);
        return 0;
    }

    audio_inicializado = true;

    return 1;
}

Audio *carga_audio(char *ruta) 
{
    Audio *audio = (Audio*)malloc(sizeof(Audio));

    if(audio == NULL)
        return NULL;

    strcpy(audio->nombre, ruta);
    audio->cargado = false;
    audio->reproduciendo = false;
    audio->duracion = 0.0;
    
    ma_result resultado = ma_decoder_init_file(ruta, NULL, &audio->decoder);
    
    if(resultado != MA_SUCCESS) 
    {
        printf("Error al cargar audio: %s\n", ruta);
        free(audio);
        return NULL;
    }
    
    ma_uint64 frames_totales;
    ma_decoder_get_length_in_pcm_frames(&audio->decoder, &frames_totales);
    audio->duracion = (float)frames_totales / audio->decoder.outputSampleRate;
    
    audio->cargado = true;
    return audio;
}

void reproduce_audio(Audio *audio) 
{
    if(audio == NULL || !audio->cargado || !audio_inicializado) 
        return;
    
    ma_decoder_seek_to_pcm_frame(&audio->decoder, 0);
    ma_engine_play_sound(&motor_audio, audio->nombre, NULL);
    
    audio->reproduciendo = true;
}

void detiene_audio(Audio *audio) 
{
    if(audio == NULL) 
        return;

    audio->reproduciendo = false;
}

void free_audio(Audio *audio) 
{
    if(audio == NULL) 
        return;

    if(audio->cargado) 
    {
        ma_decoder_uninit(&audio->decoder);
    }

    free(audio);
}

void cierra_audio() 
{
    if(audio_inicializado) 
    {
        ma_engine_uninit(&motor_audio);
        audio_inicializado = false;
    }
}

void ajusta_volumen_global(float volumen) 
{
    if(!audio_inicializado) 
        return;

    ma_engine_set_volume(&motor_audio, volumen);
}

Dialogo *crea_dialogo(char *texto, Audio *audio) 
{
    Dialogo *dialogo = (Dialogo*)malloc(sizeof(Dialogo));

    if(dialogo == NULL)
        return NULL;
        
    strcpy(dialogo->texto, texto);
    dialogo->audio = audio;
    dialogo->tiempo_mostrado = 0.0;
    dialogo->activo = false;
    return dialogo;
}

void muestra_dialogo(Personaje *personaje, Dialogo *dialogo) 
{
    if(personaje == NULL || dialogo == NULL) 
        return;
    
    if(personaje->dialogo != NULL && personaje->dialogo->activo) 
    {
        if(personaje->dialogo->audio != NULL) 
            detiene_audio(personaje->dialogo->audio);
    }
    
    personaje->dialogo = dialogo;
    dialogo->activo = 1;
    dialogo->tiempo_mostrado = 0.0;
    
    if(dialogo->audio != NULL && dialogo->audio->cargado) 
        reproduce_audio(dialogo->audio);
    
    printf("%s dice: \"%s\"\n", personaje->nombre, dialogo->texto);
}

void oculta_dialogo(Personaje *personaje) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;
    
    if(personaje->dialogo->audio != NULL)
        detiene_audio(personaje->dialogo->audio);
    
    personaje->dialogo->activo = false;
    personaje->dialogo = NULL;
}

void actualiza_dialogo(Personaje *personaje, float tiempo) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;

    if(personaje->dialogo->activo == false) 
        return;
    
    Dialogo *dialogo = personaje->dialogo;
    dialogo->tiempo_mostrado += tiempo;
    
    if(dialogo->audio != NULL) 
    {
        if(dialogo->tiempo_mostrado >= dialogo->audio->duracion + 0.5)
            oculta_dialogo(personaje);

    } 
    
    else 
    {
        if (dialogo->tiempo_mostrado >= 3.0)
            oculta_dialogo(personaje);
    }
}

void dibuja_texto(char *texto, float x, float y) 
{
    glRasterPos2f(x, y);

    for(int i = 0; texto[i] != '\0'; i++) 
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, texto[i]);
}

void dibuja_burbuja_dialogo(Personaje *personaje) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;

    if(personaje->dialogo->activo == false) 
        return;
    
    Dialogo *dialogo = personaje->dialogo;
    
    float pos_x = personaje->punto_rotacion->x;
    float pos_y = personaje->punto_rotacion->y + 100;
    float ancho = 220;
    float alto = 70;
    
    glDisable(GL_TEXTURE_2D);
    
    //Sombra
    glColor4f(0.0, 0.0, 0.0, 0.3);
    glBegin(GL_POLYGON);
    glVertex2f(pos_x - ancho/2 + 2, pos_y - alto/2 - 2);
    glVertex2f(pos_x + ancho/2 + 2, pos_y - alto/2 - 2);
    glVertex2f(pos_x + ancho/2 + 2, pos_y + alto/2 - 2);
    glVertex2f(pos_x - ancho/2 + 2, pos_y + alto/2 - 2);
    glEnd();
    
    //Borde
    glColor3f(0.0, 0.0, 0.0);
    glLineWidth(2.0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(pos_x - ancho/2, pos_y - alto/2);
    glVertex2f(pos_x + ancho/2, pos_y - alto/2);
    glVertex2f(pos_x + ancho/2, pos_y + alto/2);
    glVertex2f(pos_x - ancho/2, pos_y + alto/2);
    glEnd();
    
    //Fondo blanco
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_POLYGON);
    glVertex2f(pos_x - ancho/2, pos_y - alto/2);
    glVertex2f(pos_x + ancho/2, pos_y - alto/2);
    glVertex2f(pos_x + ancho/2, pos_y + alto/2);
    glVertex2f(pos_x - ancho/2, pos_y + alto/2);
    glEnd();
    
    //Pico
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    glVertex2f(pos_x - 15, pos_y - alto/2);
    glVertex2f(pos_x, pos_y - alto/2 - 20);
    glVertex2f(pos_x + 5, pos_y - alto/2);
    glEnd();
    
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_TRIANGLES);
    glVertex2f(pos_x - 15, pos_y - alto/2);
    glVertex2f(pos_x, pos_y - alto/2 - 20);
    glVertex2f(pos_x + 5, pos_y - alto/2);
    glEnd();
    
    //Texto
    glColor3f(0.0, 0.0, 0.0);
    dibuja_texto(dialogo->texto, pos_x - ancho/2 + 10, pos_y + 10);
    
    //Indica voz activa
    if(dialogo->audio != NULL && dialogo->audio->reproduciendo) 
    {
        float pulso = 0.5 + 0.5 * sin(dialogo->tiempo_mostrado * 10);
        float radio = 4.0 + pulso * 2.0;
        
        glColor3f(0.0, 1.0, 0.0);
        glBegin(GL_POLYGON);

        for(int i = 0; i < 20; i++) 
        {
            float angulo = 2.0 * PI * i / 20;
            glVertex2f(pos_x + ancho/2 - 15 + radio * cos(angulo), pos_y + alto/2 - 10 + radio * sin(angulo));
        }

        glEnd();
    }
    
    glLineWidth(1.0);
}

void renderiza_dialogos_jerarquia(NodoJerarquia *nodo) {
    if(nodo == NULL || !nodo->activo)
        return;
    
    if(nodo->tipo == 1 && nodo->dato != NULL) {
        Personaje *personaje = (Personaje*)nodo->dato;
        if(personaje->dialogo != NULL && personaje->dialogo->activo) {
            dibuja_burbuja_dialogo(personaje);
        }
    }
    
    NodoJerarquia *hijo = nodo->hijo;
    while(hijo != NULL) {
        renderiza_dialogos_jerarquia(hijo);
        hijo = hijo->hermano;
    }
}

void renderiza_dialogos_frame(Frame *frame) 
{
    if(frame == NULL || frame->arbol_jerarquia == NULL)
        return;
    
    renderiza_dialogos_jerarquia(frame->arbol_jerarquia);
}

void actualiza_dialogos_jerarquia(NodoJerarquia *nodo, float tiempo) 
{
    if(nodo == NULL || !nodo->activo)
        return;

    if(nodo->tipo == 1 && nodo->dato != NULL)
        actualiza_dialogo((Personaje*)nodo->dato, tiempo);
    
    NodoJerarquia *hijo = nodo->hijo;
    
    while(hijo != NULL) 
    {
        actualiza_dialogos_jerarquia(hijo, tiempo);
        hijo = hijo->hermano;
    }
}

void actualiza_dialogos_frame(Frame *frame, float tiempo) 
{
    if(frame == NULL || frame->arbol_jerarquia == NULL)
        return;
    
    actualiza_dialogos_jerarquia(frame->arbol_jerarquia, tiempo);
}

Personaje *crea_personaje(int id, char *nombre, Punto *punto_rot) 
{
    Personaje *p = (Personaje*)malloc(sizeof(Personaje));

    if(p == NULL)
        return NULL;

    p->id = id;
    strcpy(p->nombre, nombre);
    
    p->punto_rotacion = (Punto*)malloc(sizeof(Punto));
    *p->punto_rotacion = *punto_rot;
    
    p->puntos_figura = NULL;
    p->num_puntos = 0;
    p->angulo_actual = 0.0;
    p->escala_x = 1.0;
    p->escala_y = 1.0;
    p->textura = NULL;
    p->dialogo = NULL;
    
    p->padre = NULL;
    p->hijo = NULL;
    p->hermano = NULL;
    
    return p;
}

Personaje *crea_parte_personaje(char *nombre, Punto *punto_rot, Punto **vertices, int num_vertices) 
{
    Personaje *parte = (Personaje*)malloc(sizeof(Personaje));

    if(parte == NULL)
        return NULL;
        
    strcpy(parte->nombre, nombre);
    
    parte->punto_rotacion = (Punto*)malloc(sizeof(Punto));
    *parte->punto_rotacion = *punto_rot;
    
    parte->num_puntos = num_vertices;
    parte->puntos_figura = (Punto*)malloc(num_vertices * sizeof(Punto));

    for (int i = 0; i < num_vertices; i++) 
        parte->puntos_figura[i] = *vertices[i];
    
    parte->angulo_actual = 0.0;
    parte->escala_x = 1.0;
    parte->escala_y = 1.0;
    parte->textura = NULL;
    parte->dialogo = NULL;
    
    parte->padre = NULL;
    parte->hijo = NULL;
    parte->hermano = NULL;
    
    return parte;
}

void agrega_hijo_personaje(Personaje *padre, Personaje *hijo)
{
    hijo->padre = padre;
    hijo->hermano = NULL;
    
    if(padre->hijo == NULL) 
        padre->hijo = hijo;
    
    else 
    {
        Personaje *ultimo = padre->hijo;

        while (ultimo->hermano != NULL)
            ultimo = ultimo->hermano;

        ultimo->hermano = hijo;
    }
}

Personaje *busca_parte_personaje(Personaje *raiz, char *nombre) 
{
    if(raiz == NULL) 
        return NULL;

    if(strcmp(raiz->nombre, nombre) == 0) 
        return raiz;
    
    Personaje *hijo = raiz->hijo;

    while(hijo != NULL)
    {
        Personaje *resultado = busca_parte_personaje(hijo, nombre);
        
        if(resultado != NULL)
            return resultado;

        hijo = hijo->hermano;
    }

    return NULL;
}

void renderiza_personaje(Personaje *parte) 
{
    if(parte == NULL) 
        return;
    
    glPushMatrix();
    
    glTranslatef(parte->punto_rotacion->x, parte->punto_rotacion->y, parte->punto_rotacion->z);
    glRotatef(parte->angulo_actual, 0.0, 0.0, 1.0);
    glScalef(parte->escala_x, parte->escala_y, 1.0);
    
    if(parte->puntos_figura != NULL && parte->num_puntos > 0) 
    {
        if(parte->textura != NULL) 
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, parte->textura->id_textura);
            glColor4f(1.0, 1.0, 1.0, 1.0);

        }
        
        else 
        {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.8, 0.6, 0.4);
        }
        
        glBegin(GL_POLYGON);

        for(int i = 0; i < parte->num_puntos; i++) 
        {
            if (parte->textura != NULL) 
            {
                glTexCoord2f(parte->puntos_figura[i].u, parte->puntos_figura[i].v);
            }

            glVertex3f(parte->puntos_figura[i].x, parte->puntos_figura[i].y, parte->puntos_figura[i].z);
        }

        glEnd();
    }
    
    Personaje *hijo = parte->hijo;

    while(hijo != NULL) 
    {
        renderiza_personaje(hijo);
        hijo = hijo->hermano;
    }
    
    glPopMatrix();
}

Personaje *clona_personaje(Personaje *parte) 
{
    if(parte == NULL) 
        return NULL;
    
    Personaje *clon = crea_personaje(parte->id, parte->nombre, parte->punto_rotacion);
    
    if(parte->puntos_figura != NULL) 
    {
        clon->num_puntos = parte->num_puntos;
        clon->puntos_figura = (Punto*)malloc(parte->num_puntos * sizeof(Punto));

        for(int i = 0; i < parte->num_puntos; i++)
            clon->puntos_figura[i] = parte->puntos_figura[i];
    }
    
    clon->angulo_actual = parte->angulo_actual;
    clon->escala_x = parte->escala_x;
    clon->escala_y = parte->escala_y;
    clon->textura = parte->textura;
    clon->dialogo = parte->dialogo;
    
    Personaje *hijo = parte->hijo;

    while(hijo != NULL) 
    {
        Personaje *hijo_clonado = clona_personaje(hijo);
        agrega_hijo_personaje(clon, hijo_clonado);
        hijo = hijo->hermano;
    }
    
    return clon;
}

void free_personaje(Personaje *parte) 
{
    if(parte == NULL) 
        return;
    
    Personaje *hijo = parte->hijo;

    while(hijo != NULL) 
    {
        Personaje *siguiente = hijo->hermano;
        free_personaje(hijo);
        hijo = siguiente;
    }
    
    if(parte->punto_rotacion) 
        free(parte->punto_rotacion);

    if(parte->puntos_figura) 
        free(parte->puntos_figura);

    free(parte);
}


NodoJerarquia *crea_nodo_jerarquia(int id, int tipo, void *dato) 
{
    NodoJerarquia *nodo = (NodoJerarquia*)malloc(sizeof(NodoJerarquia));

    if(nodo == NULL)
        return NULL;

    nodo->id_jerarquia = id;
    nodo->tipo = tipo;
    nodo->dato = dato;
    
    nodo->pos_x = nodo->pos_y = nodo->pos_z = 0.0;
    nodo->rot_x = nodo->rot_y = nodo->rot_z = 0.0;
    nodo->escala = 1.0;
    nodo->activo = true;
    
    nodo->padre = NULL;
    nodo->hijo = NULL;
    nodo->hermano = NULL;
    
    return nodo;
}

void agrega_hijo_jerarquia(NodoJerarquia *padre, NodoJerarquia *hijo) 
{
    hijo->padre = padre;
    hijo->hermano = NULL;
    
    if(padre->hijo == NULL) 
        padre->hijo = hijo;
    
    else
    {
        NodoJerarquia *ultimo = padre->hijo;
        
        while(ultimo->hermano != NULL)
            ultimo = ultimo->hermano;

        ultimo->hermano = hijo;
    }
}

NodoJerarquia *busca_nodo_jerarquia(NodoJerarquia *raiz, int id) 
{
    if(raiz == NULL) 
        return NULL;

    if(raiz->id_jerarquia == id) 
        return raiz;
    
    NodoJerarquia *hijo = raiz->hijo;

    while(hijo != NULL) 
    {
        NodoJerarquia *resultado = busca_nodo_jerarquia(hijo, id);

        if(resultado != NULL) 
            return resultado;

        hijo = hijo->hermano;
    }

    return NULL;
}

void renderiza_arbol_jerarquia(NodoJerarquia *nodo) 
{
    if(nodo == NULL || !nodo->activo) 
        return;
    
    glPushMatrix();
    
    glTranslatef(nodo->pos_x, nodo->pos_y, nodo->pos_z);
    glRotatef(nodo->rot_z, 0.0, 0.0, 1.0);
    glScalef(nodo->escala, nodo->escala, nodo->escala);
    
    if(nodo->tipo == 1 && nodo->dato != NULL)
        renderiza_personaje((Personaje*)nodo->dato);
    
    NodoJerarquia *hijo = nodo->hijo;

    while(hijo != NULL)
    {
        renderiza_arbol_jerarquia(hijo);
        hijo = hijo->hermano;
    }
    
    glPopMatrix();
}

NodoJerarquia *clona_arbol_jerarquia(NodoJerarquia *nodo) 
{
    if(nodo == NULL) 
        return NULL;
    
    void *dato_clonado = NULL;

    if(nodo->tipo == 1 && nodo->dato != NULL) 
    {
        dato_clonado = clona_personaje((Personaje*)nodo->dato);
    }
    
    NodoJerarquia *clon = crea_nodo_jerarquia(nodo->id_jerarquia, nodo->tipo, dato_clonado);
    clon->pos_x = nodo->pos_x;
    clon->pos_y = nodo->pos_y;
    clon->pos_z = nodo->pos_z;
    clon->rot_x = nodo->rot_x;
    clon->rot_y = nodo->rot_y;
    clon->rot_z = nodo->rot_z;
    clon->escala = nodo->escala;
    clon->activo = nodo->activo;
    
    NodoJerarquia *hijo = nodo->hijo;

    while(hijo != NULL) 
    {
        NodoJerarquia *hijo_clonado = clona_arbol_jerarquia(hijo);
        agrega_hijo_jerarquia(clon, hijo_clonado);
        hijo = hijo->hermano;
    }
    
    return clon;
}

void free_arbol_jerarquia(NodoJerarquia *nodo) 
{
    if(nodo == NULL) 
        return;
    
    NodoJerarquia *hijo = nodo->hijo;

    while (hijo != NULL)
    {
        NodoJerarquia *siguiente = hijo->hermano;
        free_arbol_jerarquia(hijo);
        hijo = siguiente;
    }
    
    if(nodo->tipo == 1 && nodo->dato != NULL)
        free_personaje((Personaje*)nodo->dato);
    
    free(nodo);
}

PilaRenderizado *crea_pila_renderizado() 
{
    PilaRenderizado *pila = (PilaRenderizado*)malloc(sizeof(PilaRenderizado));

    if(pila == NULL)
        return NULL;

    pila->tope = NULL;

    return pila;
}

void push_renderizado(PilaRenderizado *pila, NodoJerarquia *nodo) 
{
    NodoPilaRenderizado *nuevo = (NodoPilaRenderizado*)malloc(sizeof(NodoPilaRenderizado));

    if(nuevo == NULL)
        return;

    nuevo->nodo = nodo;
    nuevo->sig = pila->tope;
    pila->tope = nuevo;
}

NodoJerarquia *pop_renderizado(PilaRenderizado *pila) 
{
    if(pila->tope == NULL) 
        return NULL;
    
    NodoPilaRenderizado *temp = pila->tope;
    pila->tope = pila->tope->sig;
    
    NodoJerarquia *nodo = temp->nodo;
    free(temp);

    return nodo;
}

int esta_vacia_pila_renderizado(PilaRenderizado *pila) 
{
    return pila->tope == NULL;
}

void free_pila_renderizado(PilaRenderizado *pila) 
{
    while(pila->tope != NULL) 
    {
        NodoPilaRenderizado *temp = pila->tope;
        pila->tope = pila->tope->sig;
        free(temp);
    }

    free(pila);
}

void inserta_pila_renderizado(NodoJerarquia *raiz, PilaRenderizado *pila) 
{
    if(raiz == NULL || !raiz->activo) 
        return;
    
    NodoJerarquia *hijo = raiz->hijo;

    while(hijo != NULL)
    {
        inserta_pila_renderizado(hijo, pila);
        hijo = hijo->hermano;
    }
    
    push_renderizado(pila, raiz);
}


Frame *crea_frame(int id, NodoJerarquia *arbol, double duracion) 
{
    Frame *frame = (Frame*)malloc(sizeof(Frame));

    if(frame == NULL)
        return NULL;
    
    frame->id_frame = id;
    frame->tiempo_duracion = duracion;
    frame->arbol_jerarquia = clona_arbol_jerarquia(arbol);
    
    frame->pila_renderizado = crea_pila_renderizado();
    inserta_pila_renderizado(frame->arbol_jerarquia, frame->pila_renderizado);
    
    frame->sig = NULL;

    return frame;
}

void renderiza_frame(Frame *frame) 
{
    if(frame == NULL) 
        return;
    
    while(!esta_vacia_pila_renderizado(frame->pila_renderizado)) 
    {
        NodoJerarquia *nodo = pop_renderizado(frame->pila_renderizado);
        
        if(nodo->activo) 
        {
            glPushMatrix();
            
            glTranslatef(nodo->pos_x, nodo->pos_y, nodo->pos_z);
            glRotatef(nodo->rot_z, 0.0, 0.0, 1.0);
            glScalef(nodo->escala, nodo->escala, nodo->escala);
            
            if(nodo->tipo == 1 && nodo->dato != NULL) 
            {
                renderiza_personaje((Personaje*)nodo->dato);
            }
            
            glPopMatrix();
        }
    }
    
    frame->pila_renderizado = crea_pila_renderizado();
    inserta_pila_renderizado(frame->arbol_jerarquia, frame->pila_renderizado);
}

void free_frame(Frame *frame) 
{
    if(frame == NULL) 
        return;
    
    if(frame->arbol_jerarquia)
        free_arbol_jerarquia(frame->arbol_jerarquia);

    if(frame->pila_renderizado)
        free_pila_renderizado(frame->pila_renderizado);

    free(frame);
}

Escena *crea_escena(int id, char *nombre) 
{
    Escena *escena = (Escena*)malloc(sizeof(Escena));

    if(escena == NULL)
        return NULL;

    escena->id_escena = id;
    strcpy(escena->nombre, nombre);
    escena->primer_frame = NULL;
    escena->duracion_total = 0.0;
    escena->sig = NULL;

    return escena;
}

void agrega_frame_escena(Escena *escena, Frame *frame) 
{
    frame->sig = NULL;
    
    if(escena->primer_frame == NULL) 
        escena->primer_frame = frame;
    
    else
    {
        Frame *actual = escena->primer_frame;

        while(actual->sig != NULL)
            actual = actual->sig;

        actual->sig = frame;
    }

    escena->duracion_total += frame->tiempo_duracion;
}

void free_escena(Escena *escena) 
{
    if(escena == NULL) 
        return;
    
    Frame *frame = escena->primer_frame;

    while(frame != NULL) 
    {
        Frame *siguiente = frame->sig;
        free_frame(frame);
        frame = siguiente;
    }

    free(escena);
}


Pelicula *crea_pelicula() 
{
    Pelicula *pelicula = (Pelicula*)malloc(sizeof(Pelicula));

    if(pelicula == NULL)
        return NULL;

    pelicula->frente = NULL;
    pelicula->final = NULL;
    pelicula->num_escenas = 0;
    return pelicula;
}

void encola_escena(Pelicula *pelicula, Escena *escena) 
{
    escena->sig = NULL;
    
    if(pelicula->final == NULL)
        pelicula->frente = pelicula->final = escena;
    
    else 
    {
        pelicula->final->sig = escena;
        pelicula->final = escena;
    }

    pelicula->num_escenas++;
}

Escena *desencola_escena(Pelicula *pelicula) 
{
    if(pelicula->frente == NULL) 
        return NULL;
    
    Escena *temp = pelicula->frente;
    pelicula->frente = pelicula->frente->sig;
    
    if (pelicula->frente == NULL)
        pelicula->final = NULL;
    
    pelicula->num_escenas--;

    return temp;
}

void free_pelicula(Pelicula *pelicula) 
{
    if(pelicula == NULL) 
        return;
    
    Escena *escena = pelicula->frente;

    while(escena != NULL) 
    {
        Escena *siguiente = escena->sig;
        free_escena(escena);
        escena = siguiente;
    }

    free(pelicula);
}


PilaFrames *crea_pila_frames() 
{
    PilaFrames *pila = (PilaFrames*)malloc(sizeof(PilaFrames));

    if(pila == NULL)
        return NULL;

    pila->tope = NULL;
    pila->limite = -1;
    return pila;
}

void push_pila_frame(PilaFrames *pila, Frame *frame, Escena *escena) 
{
    NodoPilaFrame *nuevo = (NodoPilaFrame*)malloc(sizeof(NodoPilaFrame));

    if(nuevo == NULL) 
    {
        puts("Error: No se pudo asignar memoria para la pila de frames");
        return;
    }

    nuevo->frame_actual = frame;
    nuevo->escena_actual = escena;
    nuevo->sig = pila->tope;
    pila->tope = nuevo;
}

int cuenta_frames_pila(PilaFrames *pila) 
{
    int cant = 0;

    NodoPilaFrame *temp = pila->tope;
    
    while(temp != NULL) 
    {
        cant++;
        temp = temp->sig;
    }
    
    return cant;
}

Frame *pop_pila_frame(PilaFrames *pila, Escena **escena_out) 
{
    if(pila->tope == NULL) 
        return NULL;
    
    NodoPilaFrame *temp = pila->tope;
    pila->tope = pila->tope->sig;
    
    Frame *frame = temp->frame_actual;
    *escena_out = temp->escena_actual;
    free(temp);
    
    return frame;
}

void free_pila_frames(PilaFrames *pila) 
{
    if(pila == NULL) 
        return;
    
    while(pila->tope != NULL) 
    {
        NodoPilaFrame *temp = pila->tope;
        pila->tope = pila->tope->sig;
        free(temp);
    }

    free(pila);
}

ColaRecursos *crea_cola_recursos() 
{
    ColaRecursos *cola = (ColaRecursos*)malloc(sizeof(ColaRecursos));

    if(cola == NULL)
        return NULL;

    cola->frente = NULL;
    cola->final = NULL;

    return cola;
}

void encola_recurso(ColaRecursos *cola, char *ruta, int tipo) 
{
    NodoRecurso *nuevo = (NodoRecurso*)malloc(sizeof(NodoRecurso));

    if(nuevo == NULL)
        return;

    strcpy(nuevo->ruta, ruta);
    nuevo->tipo = tipo;
    nuevo->dato_cargado = NULL;
    nuevo->sig = NULL;
    
    if (cola->final == NULL)
        cola->frente = cola->final = nuevo;
    
    else 
    {
        cola->final->sig = nuevo;
        cola->final = nuevo;
    }
}

NodoRecurso *desencola_recurso(ColaRecursos *cola) 
{
    if(cola->frente == NULL) 
        return NULL;
    
    NodoRecurso *temp = cola->frente;
    cola->frente = cola->frente->sig;

    if(cola->frente == NULL) 
        cola->final = NULL;
    
    return temp;
}

void cargar_recursos(ColaRecursos *cola) 
{
    puts("Recursos Cargados");
    
    NodoRecurso *actual = cola->frente;

    while(actual != NULL) 
    {
        if(actual->tipo == 0) 
        {
            if (strstr(actual->ruta, ".png") != NULL) 
            {
                actual->dato_cargado = carga_textura_transparente(actual->ruta);
            } 
            
            else 
            {
                actual->dato_cargado = carga_textura(actual->ruta);
            }

        }
        
        else if (actual->tipo == 2)
            actual->dato_cargado = carga_audio(actual->ruta);
        
        if(actual->dato_cargado == NULL) 
            printf("No se pudo cargar %s\n", actual->ruta);
        
        actual = actual->sig;
    }
    
}

NodoRecurso *busca_recurso(ColaRecursos *cola, char *ruta) 
{
    NodoRecurso *actual = cola->frente;

    while(actual != NULL) 
    {
        if(strcmp(actual->ruta, ruta) == 0)
            return actual;

        actual = actual->sig;
    }
    
    return NULL;
}

void free_cola_recursos(ColaRecursos *cola) 
{
    if(cola == NULL) 
        return;
    
    NodoRecurso *actual = cola->frente;

    while(actual != NULL) 
    {
        NodoRecurso *siguiente = actual->sig;
        
        if(actual->dato_cargado != NULL) 
        {
            if(actual->tipo == 0)
                free_textura((Textura*)actual->dato_cargado);
            
            else if(actual->tipo == 2)
                free_audio((Audio*)actual->dato_cargado);
        }
        
        free(actual);
        actual = siguiente;
    }

    free(cola);
}

void pausa()
{
    en_pausa = !en_pausa;
    
    if(en_pausa) 
        puts("=== PAUSA ===");

    else 
    {
        puts("=== REANUDADO ===");
        ultimo_tiempo = glutGet(GLUT_ELAPSED_TIME);
    }
    
    glutSetWindow(ventana_controles);
    glutPostRedisplay();
}

void retroceder() 
{
    if(pila_deshacer != NULL && pila_deshacer->tope != NULL) 
    {
        Escena *escena_anterior;
        frame_actual = pop_pila_frame(pila_deshacer, &escena_anterior);
        escena_actual = escena_anterior;
        tiempo_acumulado = 0.0;
    }

    else 
        puts("No hay frames anteriores para retroceder");
}


void reinicia_pelicula() 
{
    if(pelicula_global == NULL) 
    {
        puts("Error: No existe una peliculaa");
        return;
    }

    int frames_guardados = cuenta_frames_pila(pila_deshacer);
    
    free_pila_frames(pila_deshacer);
    
    pila_deshacer = crea_pila_frames();
    
    printf("Frames limpiados del historial: %d\n", frames_guardados);
    
    escena_actual = pelicula_global->frente;
    
    if(escena_actual != NULL) 
    {
        frame_actual = escena_actual->primer_frame;
        tiempo_acumulado = 0.0;
        ultimo_tiempo = glutGet(GLUT_ELAPSED_TIME);
        
        en_pausa = false;
        
        printf("Película reiniciada desde el inicio\n");
        printf("Escena actual: %s\n", escena_actual->nombre);
        
        if(frame_actual != NULL)
            printf("Frame actual: %d\n", frame_actual->id_frame);
    } 
    else 
    {
        printf("Advertencia: No hay escenas en la película\n");
        frame_actual = NULL;
    }
    
    printf("============================\n\n");
}

void salir()
{
    puts("FIN");
    exit(0);
}

void inicializa_botones() 
{
    float espacio = 10.0;
    float ancho_boton = 90.0;
    float alto_boton = 40.0;
    float y_pos = 30.0;
    float x_inicio = 30.0;
    
    //Boton para reanudar o pausar
    botones[0].x = x_inicio;
    botones[0].y = y_pos;
    botones[0].ancho = ancho_boton;
    botones[0].alto = alto_boton;
    strcpy(botones[0].texto, en_pausa ? "Reanudar" : "Pausa");
    botones[0].accion = pausa;
    botones[0].hover = false;
    
    //Boton para retroceder
    botones[1].x = x_inicio + ancho_boton + espacio;
    botones[1].y = y_pos;
    botones[1].ancho = ancho_boton;
    botones[1].alto = alto_boton;
    strcpy(botones[1].texto, "<<");
    botones[1].accion = retroceder;
    botones[1].hover = false;
    
    //boton para reiniciar
    botones[2].x = x_inicio + (ancho_boton + espacio) * 2;
    botones[2].y = y_pos;
    botones[2].ancho = ancho_boton;
    botones[2].alto = alto_boton;
    strcpy(botones[2].texto, "Reiniciar");
    botones[2].accion = reinicia_pelicula;
    botones[2].hover = false;
    
    //boton para salir
    botones[3].x = x_inicio + (ancho_boton + espacio) * 3;
    botones[3].y = y_pos;
    botones[3].ancho = ancho_boton;
    botones[3].alto = alto_boton;
    strcpy(botones[3].texto, "Salir");
    botones[3].accion = salir;
    botones[3].hover = false;
}

void dibuja_boton(Boton *btn) 
{
    glDisable(GL_TEXTURE_2D);
    
    //Color del boton si el mouse esta encima de el
    if(btn->hover) 
    {
        glColor3f(0.3, 0.5, 0.8);
    }
    
    else 
    {
        glColor3f(0.2, 0.3, 0.6);
    }
    
    //Fondo del boton
    glBegin(GL_POLYGON);
    glVertex2f(btn->x, btn->y);
    glVertex2f(btn->x + btn->ancho, btn->y);
    glVertex2f(btn->x + btn->ancho, btn->y + btn->alto);
    glVertex2f(btn->x, btn->y + btn->alto);
    glEnd();
    
    //Borde del boton
    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(2.0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(btn->x, btn->y);
    glVertex2f(btn->x + btn->ancho, btn->y);
    glVertex2f(btn->x + btn->ancho, btn->y + btn->alto);
    glVertex2f(btn->x, btn->y + btn->alto);
    glEnd();
    glLineWidth(1.0);
    
    //Texto del boton

    glColor3f(1.0, 1.0, 1.0);
    int texto_ancho = 0;

    for(int i = 0; btn->texto[i] != '\0'; i++)
        texto_ancho += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, btn->texto[i]);
    
    float texto_x = btn->x + (btn->ancho - texto_ancho) / 2;
    float texto_y = btn->y + btn->alto / 2 - 6;
    
    glRasterPos2f(texto_x, texto_y);

    for(int i = 0; btn->texto[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, btn->texto[i]);
}

void display_controles() 
{
    glutSetWindow(ventana_controles);
    
    glClearColor(0.15, 0.15, 0.15, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 100);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    //Actualiza el texto del boton de pausa
    strcpy(botones[0].texto, en_pausa ? "Reanudar" : "Pausa");
    
    //Dibuja todos los botones
    for(int i = 0; i < num_botones; i++)
        dibuja_boton(&botones[i]);
    
    //Cantidad de frames
    glColor3f(0.8, 0.8, 0.8);
    glRasterPos2f(450, 70);
    char info[100];

    sprintf(info, "Frames: %d", cuenta_frames_pila(pila_deshacer));

    for(int i = 0; info[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info[i]);
    
    //Escena
    if(escena_actual != NULL) 
    {
        glRasterPos2f(450, 50);
        sprintf(info, "Escena: %s", escena_actual->nombre);

        for(int i = 0; info[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info[i]);
    }
    
    //Estado de ela pelicula
    glRasterPos2f(450, 30);
    sprintf(info, "Estado: %s", en_pausa ? "PAUSADO" : "REPRODUCIENDOSE");

    for(int i = 0; info[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info[i]);
    
    glutSwapBuffers();
}

//Checa si el mouse esta encima del boton
bool punto_en_boton(Boton *btn, int x, int y) 
{
    return (x >= btn->x && x <= btn->x + btn->ancho && y >= btn->y && y <= btn->y + btn->alto);
}

void mouse_controles(int button, int state, int x, int y) 
{
    if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) 
    {
        //Convierte coordenadas
        int y_invertido = 100 - y;
        
        //Verifica que botón fue clickeado
        for(int i = 0; i < num_botones; i++) 
        {
            if(punto_en_boton(&botones[i], x, y_invertido)) 
            {
                printf("Boton clickeado: %s\n", botones[i].texto);
                
                //Ejecuta la accion del boton
                if(botones[i].accion != NULL)
                    botones[i].accion();
                
                //Actualiza ambas ventanas
                glutSetWindow(ventana_controles);
                glutPostRedisplay();
                glutSetWindow(ventana_principal);
                glutPostRedisplay();
                
                break;
            }
        }
    }
}

//Controles de movimiento del mouse
void motion_controles(int x, int y) 
{
    int y_invertido = 100 - y;
    bool cambio = false;
    
    for(int i = 0; i < num_botones; i++) 
    {
        bool hover_anterior = botones[i].hover;
        botones[i].hover = punto_en_boton(&botones[i], x, y_invertido);
        
        if(hover_anterior != botones[i].hover)
            cambio = true;
    }
    
    if(cambio) 
    {
        glutSetWindow(ventana_controles);
        glutPostRedisplay();
    }
}

void display() 
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 1200, 0, 1000);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    if(frame_actual != NULL) 
    {
        renderiza_frame(frame_actual);
        renderiza_dialogos_frame(frame_actual);
    }
    
    //Muestra el simbolo de de pausa en pantalla
    if(en_pausa) 
    {
        glDisable(GL_TEXTURE_2D);
        
        //Fondo semi-transparente oscuro
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0, 0.0, 0.0, 0.5);
        glBegin(GL_POLYGON);
        glVertex2f(0, 0);
        glVertex2f(1200, 0);
        glVertex2f(1200, 1000);
        glVertex2f(0, 600);
        glEnd();
        
        //Simbolo de pausa
        glColor3f(1.0, 1.0, 1.0);
        
        //Barra izquierda
        glBegin(GL_POLYGON);
        glVertex2f(360, 250);
        glVertex2f(380, 250);
        glVertex2f(380, 350);
        glVertex2f(360, 350);
        glEnd();
        
        //Barra derecha
        glBegin(GL_POLYGON);
        glVertex2f(420, 250);
        glVertex2f(440, 250);
        glVertex2f(440, 350);
        glVertex2f(420, 350);
        glEnd();
        
        //Pausa
        glColor3f(1.0, 1.0, 1.0);
        glRasterPos2f(360, 220);

        char *texto_pausa = "PAUSA";

        for(int i = 0; texto_pausa[i] != '\0'; i++) 
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, texto_pausa[i]);
    }
    
    glutSwapBuffers();
}

void actualiza(int valor) 
{
    int tiempo_actual = glutGet(GLUT_ELAPSED_TIME);

    float tiempo = (tiempo_actual - ultimo_tiempo) / 1000.0;

    ultimo_tiempo = tiempo_actual;
    
    //Si esta en pausa solo actualiza el display sin avanzar frames
    if(en_pausa) 
    {
        glutSetWindow(ventana_principal);
        glutPostRedisplay();
        glutSetWindow(ventana_controles);
        glutPostRedisplay();
        glutTimerFunc(33, actualiza, 0);
        return;
    }
    
    if(escena_actual != NULL && frame_actual != NULL)
    {
        tiempo_acumulado += tiempo;
        
        //Actualizr dialogos
        actualiza_dialogos_frame(frame_actual, tiempo);
        
        //Cambia al siguiente frame cuando se cumple la duracion
        if(tiempo_acumulado >= frame_actual->tiempo_duracion) 
        {
            tiempo_acumulado = 0.0;
            
            //Guarda el frame actual en pila de frames
            push_pila_frame(pila_deshacer, frame_actual, escena_actual);
            
            frame_actual = frame_actual->sig;
            
            //Si termino la escena pasa a la siguiente
            if(frame_actual == NULL) 
            {
                escena_actual = escena_actual->sig;

                if(escena_actual != NULL) 
                    frame_actual = escena_actual->primer_frame;
                
                else 
                    printf("FIN DE LA PELICULA");
            }
        }
    }
    
    glutSetWindow(ventana_principal);
    glutPostRedisplay();
    glutSetWindow(ventana_controles);
    glutPostRedisplay();

    glutTimerFunc(33, actualiza, 0); //33ms son aprox 30FPS
}

void keyboard(unsigned char key, int x, int y) 
{
    switch(key) 
    {
        //Retrocede frame
        case 'r':
        case 'R':

            retroceder();
            break;

        case 'p':
        case 'P':

            pausa();
            break;

        case 'x':
        case 'X':
            reinicia_pelicula();
            break;

        case 27:
            salir();
            break;
    }
}

void convierte_absolutas_a_relativas_personaje(Personaje *parte, double padre_x, double padre_y) 
{
    if(parte == NULL) 
        return;
    
    //Guarda las coordenadas absolutas originales del punto de rotacion
    double x_absoluto = parte->punto_rotacion->x;
    double y_absoluto = parte->punto_rotacion->y;
    
    //Convierte las coordenadas del punto de rotacion a coordenadas relativas
    parte->punto_rotacion->x -= padre_x;
    parte->punto_rotacion->y -= padre_y;
    
    //Convierte todos los puntos de la figura relativos a su punto de rotacion
    for(int i = 0; i < parte->num_puntos; i++) 
    {
        parte->puntos_figura[i].x -= x_absoluto;
        parte->puntos_figura[i].y -= y_absoluto;
    }
    
    Personaje *hijo = parte->hijo;

    while(hijo != NULL) 
    {
        convierte_absolutas_a_relativas_personaje(hijo, x_absoluto, y_absoluto);
        hijo = hijo->hermano;
    }
}



int main(int argc, char** argv) 
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 50);
    ventana_principal = glutCreateWindow("El viaje de Mr. Atomix");
    
    glClearColor(0.5, 0.7, 1.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    
    //Ventana de controles
    glutInitWindowSize(800, 100);
    glutInitWindowPosition(100, 660);
    ventana_controles = glutCreateWindow("Controles del Reproductor");
    
    glClearColor(0.15, 0.15, 0.15, 1.0);
    
    glutDisplayFunc(display_controles);
    glutMouseFunc(mouse_controles);
    glutMotionFunc(motion_controles);
    glutPassiveMotionFunc(motion_controles);

    if(!inicializa_audio()) 
    {
        puts("Error al inicializar audio");
        return 1;
    }
    
    ajusta_volumen_global(0.7);
    
    ColaRecursos *cola_recursos = crea_cola_recursos();
    cargar_recursos(cola_recursos);

    pelicula_global = crea_pelicula();

    escena_actual = pelicula_global->frente;
    
    if(escena_actual != NULL)
        frame_actual = escena_actual->primer_frame;

    pila_deshacer = crea_pila_frames();
    ultimo_tiempo = glutGet(GLUT_ELAPSED_TIME);
    en_pausa = false;
    
    inicializa_botones();
    
    //Timer global
    glutTimerFunc(33, actualiza, 0);
    
    glutMainLoop();
    
    cierra_audio();
    free_cola_recursos(cola_recursos);
    free_pelicula(pelicula_global);
    free_pila_frames(pila_deshacer);
    
    return 0;
}
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
    int tipo; //Bandera que indica que estructura tiene almacenada dentro del nodo de dato 1 = Personaje, 2 = Material, 3 = Luz
    
    //Transformaciones locales
    double pos_x, pos_y, pos_z;
    double rot_x, rot_y, rot_z;
    double escala;
    
    void *dato; //Puntero void a personaje, material o luz
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
    int id_textura;
    char nombre[100];
    int ancho;
    int alto;
}Textura;

typedef struct Audio 
{
    ma_decoder decoder; //Decodificador de miniaudio
    ma_sound sound;
    char nombre[100];
    float duracion;
    bool cargado;
    bool reproduciendo;
    bool loop;
}Audio;

typedef struct Dialogo 
{
    char lineas[5][100]; 
    int num_lineas;
    int total_caracteres;
    Audio *audio;
    float tiempo_mostrado;
    bool activo;
    bool audio_pausado;
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

typedef struct Material 
{
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float shininess;
    bool activo;
}Material;

typedef struct Luz 
{
    int id_luz;
    float posicion[4];
    float ambiental[4];
    float difusa[4];
    float especular[4];
    bool activa;
}Luz;

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
    Material *material;
    
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

typedef struct Camara 
{
    double pos_x;
    double pos_y;
    double zoom;
    double objetivo_x;
    double objetivo_y;
    double objetivo_zoom;
    double suavidad; //Velocidad con la que sigue la camara
}Camara;

Camara camara_global;
bool camara_sigue_personaje = true;

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
Boton botones[5];
int num_botones = 5;

ColaRecursos *cola_recursos_global = NULL;

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
    //Mantiene la cache para evitar cargar la misma textura multiples veces
    static Textura *texturas_cargadas[100];
    static int num_texturas = 0;
    
    //Verifica si la textura ya esta cargada
    for(int i = 0; i < num_texturas; i++) 
    {
        if(strcmp(texturas_cargadas[i]->nombre, ruta) == 0)
            return texturas_cargadas[i];
    }
    
    //Carga nueva textura
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
        printf("Error cargando textura %s: %s\n", ruta, SOIL_last_result());
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
    
    printf("  -> Textura cargada exitosamente: %s (ID: %d, %dx%d)\n", ruta, tex->id_textura, tex->ancho, tex->alto);
    
    //Almacena en cache para reutilizar y no tener que buscarla varias veces
    if(num_texturas < 100)
        texturas_cargadas[num_texturas++] = tex;
    
    return tex;
}

Textura *carga_textura_transparente(char *ruta) 
{
    //Mantiene la cache para evitar cargar la misma textura multiples veces
    static Textura *texturas_cargadas[100];
    static int num_texturas = 0;
    
    //Verifica si la textura ya esta cargada
    for(int i = 0; i < num_texturas; i++) 
    {
        if(strcmp(texturas_cargadas[i]->nombre, ruta) == 0)
            return texturas_cargadas[i];
    }
    
    //Carga nueva textura
    Textura *tex = (Textura*)malloc(sizeof(Textura));
    strcpy(tex->nombre, ruta);
    
    tex->id_textura = SOIL_load_OGL_texture
    (
        ruta,
        SOIL_LOAD_RGBA,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y
    );
    
    if(tex->id_textura == 0) 
    {
        printf("Error cargando textura transparente %s: %s\n", ruta, SOIL_last_result());
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
    
    printf("  -> Textura transparente cargada: %s (ID: %d, %dx%d)\n", 
           ruta, tex->id_textura, tex->ancho, tex->alto);
    
    //Almacena en cache para reutilizar y no tener que buscarla varias veces
    if(num_texturas < 100)
        texturas_cargadas[num_texturas++] = tex;
    
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
    audio->loop = false;
    audio->duracion = 0.0;
    
    //Primero se inicializam el decoder para obtener la duracion
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

    //Se inicializa el sonido para la reproduccion
    resultado = ma_sound_init_from_file(&motor_audio, ruta, 0, NULL, NULL, &audio->sound);

    if(resultado != MA_SUCCESS) 
    {
        printf("Error al inicializar sonido para: %s\n", ruta);
        ma_decoder_uninit(&audio->decoder);
        free(audio);
        return NULL;
    }
    
    audio->cargado = true;
    printf("Audio cargado: %s (duracion: %.2f segundos)\n", ruta, audio->duracion);
    return audio;
}

void reproduce_audio(Audio *audio) 
{
    if(audio == NULL || !audio->cargado || !audio_inicializado) 
        return;

    //Si ya se esta reproduciendo se lo detiene y vuelve a comenzar
    if(audio->reproduciendo) 
        ma_sound_stop(&audio->sound);
    
    ma_decoder_seek_to_pcm_frame(&audio->decoder, 0);

    //Configura loop si se ocupa
    if(audio->loop) 
        ma_sound_set_looping(&audio->sound, MA_TRUE);
    else 
        ma_sound_set_looping(&audio->sound, MA_FALSE);

    ma_result resultado = ma_sound_start(&audio->sound);
    
    if(resultado != MA_SUCCESS) 
    {
        printf("Error al reproducir audio: %s\n", audio->nombre);
        return;
    }
    
    audio->reproduciendo = true;
}

void detiene_audio(Audio *audio) 
{
    if(audio == NULL || !audio->cargado) 
        return;

    if(audio->reproduciendo) 
    {
        ma_sound_stop(&audio->sound);
        audio->reproduciendo = false;
    }
}

void free_audio(Audio *audio) 
{
    if(audio == NULL) 
        return;

    
    if(audio->cargado) 
    {
        if(audio->reproduciendo) 
            ma_sound_stop(&audio->sound);

        ma_sound_uninit(&audio->sound);

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
        puts("audio cerrado");
    }
}

void ajusta_volumen_global(float volumen) 
{
    if(!audio_inicializado) 
        return;

    ma_engine_set_volume(&motor_audio, volumen);
}

void ajusta_volumen_audio(Audio *audio, float volumen) 
{
    if(audio == NULL || audio->cargado == false) 
        return;
    
    ma_sound_set_volume(&audio->sound, volumen);
}

void set_audio_loop(Audio *audio, bool loop) 
{
    if(audio == NULL || !audio->cargado) 
        return;
    
    audio->loop = loop;
    ma_sound_set_looping(&audio->sound, loop ? MA_TRUE : MA_FALSE);
}

Dialogo *crea_dialogo(int n, char *textos[], Audio *audio) 
{
    Dialogo *dialogo = (Dialogo*)malloc(sizeof(Dialogo));

    if(dialogo == NULL)
        return NULL;
    
    if(n > 5) 
        n = 5;
    
    dialogo->num_lineas = n;
    dialogo->total_caracteres = 0;
    
    for(int i = 0; i < n; i++) 
    {
        strcpy(dialogo->lineas[i], textos[i]);
        dialogo->total_caracteres += strlen(textos[i]);
    }

    dialogo->audio = audio;
    dialogo->tiempo_mostrado = 0.0;
    dialogo->activo = false;
    dialogo->audio_pausado = false;
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
    dialogo->activo = true;
    dialogo->tiempo_mostrado = 0.0;
    dialogo->audio_pausado = false;
}

void oculta_dialogo(Personaje *personaje) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;
    
    if(personaje->dialogo->audio != NULL)
    {
        detiene_audio(personaje->dialogo->audio);

        //Desactiva loop
        set_audio_loop(personaje->dialogo->audio, false);

        personaje->dialogo->audio_pausado = false;
    }

    personaje->dialogo->activo = false;
    personaje->dialogo = NULL;
}

void actualiza_dialogo(Personaje *personaje, float tiempo) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;

    if(personaje->dialogo->activo == false || personaje->dialogo->audio_pausado)
        return;
    
    Dialogo *dialogo = personaje->dialogo;

    if(dialogo->audio != NULL && dialogo->audio->cargado)
    {
        if(!dialogo->audio->reproduciendo && dialogo->tiempo_mostrado <= 0.1) 
        {
            set_audio_loop(dialogo->audio, true);
            reproduce_audio(dialogo->audio);
        }
    }

    dialogo->tiempo_mostrado += tiempo;

    float tiempo_total_dialogo;
    
    if(dialogo->audio != NULL && dialogo->audio->cargado) 
        tiempo_total_dialogo = dialogo->audio->duracion + 1.0;

    else 
        tiempo_total_dialogo = 7.0;
    
    if(dialogo->tiempo_mostrado >= tiempo_total_dialogo)
        oculta_dialogo(personaje);
}

void dibuja_texto(char *texto, float x, float y) 
{
    glRasterPos2f(x, y);

    for(int i = 0; texto[i] != '\0'; i++) 
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, texto[i]);
}

void dibuja_burbuja_dialogo(Personaje *personaje, float escala_personaje) 
{
    if(personaje == NULL || personaje->dialogo == NULL) 
        return;

    if(personaje->dialogo->activo == false) 
        return;
    
    Dialogo *dialogo = personaje->dialogo;

    //Tiempo para mostrar todo el texto
    float tiempo_total_escritura = 7.0;
    int caracteres_a_mostrar = 0;

    if(dialogo->tiempo_mostrado >= tiempo_total_escritura) 
        caracteres_a_mostrar = dialogo->total_caracteres;

    else
    {
        float porcentaje = dialogo->tiempo_mostrado / tiempo_total_escritura;
        caracteres_a_mostrar = (int)(porcentaje * dialogo->total_caracteres);
    } 
    
    if(caracteres_a_mostrar <= 0)
        return;
    
    glPushMatrix(); 
    
    //Factor de ajuste basado en la escala
    float factor_ajuste = escala_personaje / 28.0;
    
    glTranslatef(10.0, 24.0, 0);
    
    //Tamaño de la burbuja
    float ancho = 15.0;
    float alto_linea = 1.2;
    float alto = 2.0 + (dialogo->num_lineas * alto_linea);

    glDisable(GL_TEXTURE_2D);
    
    //Sombra
    glColor4f(0.0, 0.0, 0.0, 0.3);
    glBegin(GL_POLYGON);
    glVertex2f(-ancho/2 + 0.2, -alto/2 - 0.2);
    glVertex2f(ancho/2 + 0.2, -alto/2 - 0.2);
    glVertex2f(ancho/2 + 0.2, alto/2 - 0.2);
    glVertex2f(-ancho/2 + 0.2, alto/2 - 0.2);
    glEnd();
    
    //Fondo blanco
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_POLYGON);
    glVertex2f(-ancho/2, -alto/2);
    glVertex2f(ancho/2, -alto/2);
    glVertex2f(ancho/2, alto/2);
    glVertex2f(-ancho/2, alto/2);
    glEnd();
    
    //Borde negro
    glColor3f(0.0, 0.0, 0.0);
    glLineWidth(2.0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-ancho/2, -alto/2);
    glVertex2f(ancho/2, -alto/2);
    glVertex2f(ancho/2, alto/2);
    glVertex2f(-ancho/2, alto/2);
    glEnd();
    
    //Pico de la burbuja
    glBegin(GL_TRIANGLES);
    glColor3f(1.0, 1.0, 1.0); 
    glVertex2f(-0.5, -alto/2);
    glVertex2f(0, -alto/2 - 1.0);
    glVertex2f(0.5, -alto/2);
    glEnd();
    
    //Contorno del pico
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    glVertex2f(-0.5, -alto/2);
    glVertex2f(0, -alto/2 - 1.0);
    glVertex2f(0.5, -alto/2);
    glEnd();
    
    //Dialogo
    glColor3f(0.0, 0.0, 0.0);
    
    float escala_texto = 0.01;
    float y_ini = (alto / 2.0) - 1.0; 
    int contador_global = 0;
    
    //Dibuja cada linea con efecto maquina de escribir
    for(int i = 0; i < dialogo->num_lineas; i++) 
    {
        float x_pos = -ancho/2 + 0.5;
        float y_pos = y_ini - (i * alto_linea);
        
        glPushMatrix();
        glTranslatef(x_pos, y_pos, 0);
        glScalef(escala_texto, escala_texto, 1.0);
        
        glRasterPos2f(0, 0);

        //Dibuja solo los caracteres que deben estar visibles
        for(int j = 0; dialogo->lineas[i][j] != '\0'; j++) 
        {
            if(contador_global < caracteres_a_mostrar) 
            {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, dialogo->lineas[i][j]);
                contador_global++;
            } 
            else 
            {
                glPopMatrix();
                glPopMatrix();
                glLineWidth(1.0);
                return;
            }
        }
        
        glPopMatrix();
    }
    
    glPopMatrix(); 
    glLineWidth(1.0);
}

void renderiza_dialogos_jerarquia(NodoJerarquia *nodo) 
{
    if(nodo == NULL || !nodo->activo)
        return;

    glPushMatrix();

    glTranslatef(nodo->pos_x, nodo->pos_y, nodo->pos_z);
    glRotatef(nodo->rot_z, 0.0, 0.0, 1.0);
    glScalef(nodo->escala, nodo->escala, nodo->escala);
    
    if(nodo->tipo == 1 && nodo->dato != NULL) 
    {
        Personaje *personaje = (Personaje*)nodo->dato;

        if(personaje->dialogo != NULL && personaje->dialogo->activo) 
        {
            dibuja_burbuja_dialogo(personaje, nodo->escala);
        }
    }
    
    NodoJerarquia *hijo = nodo->hijo;

    while(hijo != NULL) 
    {
        renderiza_dialogos_jerarquia(hijo);
        hijo = hijo->hermano;
    }

    glPopMatrix();
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
    p->material = NULL;  
    
    p->padre = NULL;
    p->hijo = NULL;
    p->hermano = NULL;
    
    return p;
}

void asigna_material_personaje(Personaje *personaje, Material *material) 
{
    if(personaje == NULL) 
        return;
    
    personaje->material = material;
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

void aplica_material(Material *mat) 
{
    if(mat == NULL || mat->activo == false) 
        return;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat->ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat->diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat->specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->shininess);
}

void aplica_luz(Luz *luz) 
{
    if(luz == NULL || luz->activa == false) 
        return;
    
    glEnable(luz->id_luz);
    glLightfv(luz->id_luz, GL_POSITION, luz->posicion);
    glLightfv(luz->id_luz, GL_AMBIENT, luz->ambiental);
    glLightfv(luz->id_luz, GL_DIFFUSE, luz->difusa);
    glLightfv(luz->id_luz, GL_SPECULAR, luz->especular);
}

void desactiva_luz(Luz *luz) 
{
    if(luz == NULL) 
        return;
    
    glDisable(luz->id_luz);
}

Material *clona_material(Material *orig) 
{
    if(orig == NULL) 
        return NULL;
    
    Material *clon = (Material*)malloc(sizeof(Material));
    
    if(clon == NULL)
        return NULL;
    
    for(int i = 0; i < 4; i++) 
    {
        clon->ambient[i] = orig->ambient[i];
        clon->diffuse[i] = orig->diffuse[i];
        clon->specular[i] = orig->specular[i];
    }
    
    clon->shininess = orig->shininess;
    clon->activo = orig->activo;
    
    return clon;
}

Luz *clona_luz(Luz *orig) 
{
    if(orig == NULL) 
        return NULL;
    
    Luz *clon = (Luz*)malloc(sizeof(Luz));
    
    if(clon == NULL)
        return NULL;
    
    clon->id_luz = orig->id_luz;
    
    for(int i = 0; i < 4; i++) 
    {
        clon->posicion[i] = orig->posicion[i];
        clon->ambiental[i] = orig->ambiental[i];
        clon->difusa[i] = orig->difusa[i];
        clon->especular[i] = orig->especular[i];
    }
    
    clon->activa = orig->activa;
    
    return clon;
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
    
    //Aplica material o luz si corresponde
    if(nodo->tipo == 2 && nodo->dato != NULL) 
        aplica_material((Material*)nodo->dato);

    else if(nodo->tipo == 3 && nodo->dato != NULL) 
        aplica_luz((Luz*)nodo->dato);

    else if(nodo->tipo == 1 && nodo->dato != NULL) 
    {
        //Si es un personaje aplica su material si tiene
        Personaje *personaje = (Personaje*)nodo->dato;

        if(personaje->material != NULL) 
        {
            aplica_material(personaje->material);
        }

        renderiza_personaje(personaje);
    }
    
    NodoJerarquia *hijo = nodo->hijo;

    while(hijo != NULL)
    {
        renderiza_arbol_jerarquia(hijo);
        hijo = hijo->hermano;
    }

    //Desactiva luz si corresponde (para no afectar otros objetos)
    if(nodo->tipo == 3 && nodo->dato != NULL) 
    {
        desactiva_luz((Luz*)nodo->dato);
    }
    
    glPopMatrix();
}

NodoJerarquia *clona_arbol_jerarquia(NodoJerarquia *nodo) 
{
    if(nodo == NULL) 
        return NULL;
    
    void *dato_clonado = NULL;

    if(nodo->tipo == 1 && nodo->dato != NULL) 
        dato_clonado = clona_personaje((Personaje*)nodo->dato);

    else if(nodo->tipo == 2 && nodo->dato != NULL) 
    {
        //Clona material
        Material *orig = (Material*)nodo->dato;
        Material *clon = (Material*)malloc(sizeof(Material));

        if(clon != NULL) 
        {
            for(int i = 0; i < 4; i++) 
            {
                clon->ambient[i] = orig->ambient[i];
                clon->diffuse[i] = orig->diffuse[i];
                clon->specular[i] = orig->specular[i];
            }

            clon->shininess = orig->shininess;
            clon->activo = orig->activo;
        }
        dato_clonado = clon;
    }

    else if(nodo->tipo == 3 && nodo->dato != NULL) 
    {
        //Clonarluz
        Luz *orig = (Luz*)nodo->dato;
        Luz *clon = (Luz*)malloc(sizeof(Luz));

        if(clon != NULL) 
        {
            clon->id_luz = orig->id_luz;
            for(int i = 0; i < 4; i++) 
            {
                clon->posicion[i] = orig->posicion[i];
                clon->ambiental[i] = orig->ambiental[i];
                clon->difusa[i] = orig->difusa[i];
                clon->especular[i] = orig->especular[i];
            }

            clon->activa = orig->activa;
        }

        dato_clonado = clon;
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
    
    if(nodo->dato != NULL) 
    {
        if(nodo->tipo == 1) 
            free_personaje((Personaje*)nodo->dato);

        else if(nodo->tipo == 2) 
            free((Material*)nodo->dato);

        else if(nodo->tipo == 3) 
            free((Luz*)nodo->dato);
    }
    
    free(nodo);
}

Material *crea_material(float amb[4], float diff[4], float spec[4], float shine) 
{
    Material *mat = (Material*)malloc(sizeof(Material));
    
    if(mat == NULL)
        return NULL;
    
    for(int i = 0; i < 4; i++) 
    {
        mat->ambient[i] = amb[i];
        mat->diffuse[i] = diff[i];
        mat->specular[i] = spec[i];
    }
    
    mat->shininess = shine;
    mat->activo = true;
    
    return mat;
}

Luz *crea_luz(int id, float pos[4], float amb[4], float diff[4], float spec[4]) 
{
    Luz *luz = (Luz*)malloc(sizeof(Luz));
    
    if(luz == NULL)
        return NULL;
    
    luz->id_luz = GL_LIGHT0 + id;
    
    for(int i = 0; i < 4; i++) 
    {
        luz->posicion[i] = pos[i];
        luz->ambiental[i] = amb[i];
        luz->difusa[i] = diff[i];
        luz->especular[i] = spec[i];
    }
    
    luz->activa = true;
    
    return luz;
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
    
    PilaRenderizado *pila_temp = crea_pila_renderizado();
    inserta_pila_renderizado(frame->arbol_jerarquia, pila_temp);
    
    NodoPilaRenderizado *actual = pila_temp->tope;
    
    while(actual != NULL) 
    {
        NodoJerarquia *nodo = actual->nodo;
        
        if(nodo->activo) 
        {
            glPushMatrix();
            
            //Aplica transformaciones locales del nodo
            glTranslatef(nodo->pos_x, nodo->pos_y, nodo->pos_z);
            glRotatef(nodo->rot_z, 0.0, 0.0, 1.0);
            glScalef(nodo->escala, nodo->escala, nodo->escala);
            
            //Aplicar material o luz si corresponde
            if(nodo->tipo == 2 && nodo->dato != NULL) 
                aplica_material((Material*)nodo->dato);

            else if(nodo->tipo == 3 && nodo->dato != NULL) 
                aplica_luz((Luz*)nodo->dato);

            else if(nodo->tipo == 1 && nodo->dato != NULL) 
            {
                //Si es un personaj, aplicar su material si tiene
                Personaje *personaje = (Personaje*)nodo->dato;
                
                if(personaje->material != NULL) 
                {
                    aplica_material(personaje->material);
                }
                
                //Renderiza el personaje
                renderiza_personaje(personaje);
            }
            
            glPopMatrix();
        }
        
        actual = actual->sig;
    }
    
    free_pila_renderizado(pila_temp);
    
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
    puts("Cargando recursos");
    
    NodoRecurso *actual = cola->frente;
    int recursos_cargados = 0;
    int errores = 0;

    while(actual != NULL) 
    {
        printf("Procesando recurso: %s (tipo: %d)\n", actual->ruta, actual->tipo);
        
        if(actual->tipo == 0)
        {
            if(strstr(actual->ruta, ".png") != NULL) 
            {
                actual->dato_cargado = carga_textura_transparente(actual->ruta);

                if(actual->dato_cargado != NULL) 
                {
                    printf("Textura transparente cargada: %s\n", actual->ruta);
                    recursos_cargados++;
                }

                else 
                {
                    printf("error al cargar textura transparente: %s\n", actual->ruta);
                    errores++;
                }
            }

            else 
            {
                actual->dato_cargado = carga_textura(actual->ruta);

                if(actual->dato_cargado != NULL) 
                {
                    printf("Textura cargada: %s\n", actual->ruta);
                    recursos_cargados++;
                }
                else 
                {
                    printf("error al cargar textura: %s\n", actual->ruta);
                    errores++;
                }
            }
        }

        else if(actual->tipo == 2)
        {
            actual->dato_cargado = carga_audio(actual->ruta);
            if(actual->dato_cargado != NULL) 
            {
                printf("Audio cargado: %s\n", actual->ruta);
                recursos_cargados++;
            }
            else 
            {
                printf("error al cargar audio: %s\n", actual->ruta);
                errores++;
            }
        }
        
        actual = actual->sig;
    }
    
    printf("Recursos cargados: %d correctos, %d errores", recursos_cargados, errores);
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

Textura *busca_textura_en_cola(ColaRecursos *cola, char *ruta) 
{
    NodoRecurso *nodo = busca_recurso(cola, ruta);
    
    if(nodo == NULL || nodo->dato_cargado == NULL) 
    {
        printf("Error: Textura no encontrada en cola: %s\n", ruta);
        return NULL;
    }
    
    return (Textura*)nodo->dato_cargado;
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

void inicializa_camara() 
{
    camara_global.pos_x = 600.0;
    camara_global.pos_y = 300.0;
    camara_global.zoom = 1.0;
    camara_global.objetivo_x = 600.0;
    camara_global.objetivo_y = 300.0;
    camara_global.objetivo_zoom = 1.0;
    camara_global.suavidad = 0.1;
}

NodoJerarquia *busca_mr_atomix_en_arbol(NodoJerarquia *nodo) 
{
    if(nodo == NULL || !nodo->activo) 
        return NULL;
    
    //Si el nodo es un personaje verificar si su nombre es torso
    if(nodo->tipo == 1 && nodo->dato != NULL) 
    {
        Personaje *p = (Personaje*)nodo->dato;
        if(p != NULL && strcmp(p->nombre, "torso") == 0) 
            return nodo;
    }
    
    NodoJerarquia *hijo = nodo->hijo;
    while(hijo != NULL) 
    {
        NodoJerarquia *resultado = busca_mr_atomix_en_arbol(hijo);
        if(resultado != NULL) 
            return resultado;
        hijo = hijo->hermano;
    }
    
    return NULL;
}

//Calcula la posicion objetivo de la camara basandose en mr atomix
void actualiza_objetivo_camara(Frame *frame) 
{
    if(frame == NULL || frame->arbol_jerarquia == NULL || !camara_sigue_personaje) 
        return;
    
    NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame->arbol_jerarquia);
    
    if(nodo_atomix != NULL) 
    {
        Personaje *atomix = (Personaje*)nodo_atomix->dato;
        double escala_personaje = nodo_atomix->escala;
        
        //Posicion del personaje en las coordenadas del mundo
        camara_global.objetivo_x = nodo_atomix->pos_x;
  
        double altura_personaje = 30.0 * escala_personaje;
        
        //Posiciona la camara en el centro vertical del personaje
        camara_global.objetivo_y = nodo_atomix->pos_y + (altura_personaje * 0.4);
        
        if(escala_personaje > 0) 
        {

            camara_global.objetivo_zoom = 1.0 / sqrt(escala_personaje / 28.0);

            if(escala_personaje < 15.0) 
            {
                //Para escalas muy pequeña, limitas el zoom maximo
                camara_global.objetivo_zoom = fmin(camara_global.objetivo_zoom, 1.8);
            }

            else if(escala_personaje > 40.0) 
            {
                //Para escalas muy grandes,limita el zoom minimo
                camara_global.objetivo_zoom = fmax(camara_global.objetivo_zoom, 0.5);
            }
        }
        
        // Limita el zoom para evitar valores extremos
        if(camara_global.objetivo_zoom < 0.4) 
            camara_global.objetivo_zoom = 0.4;

        if(camara_global.objetivo_zoom > 1.8) 
            camara_global.objetivo_zoom = 1.8;
    }
}

void actualiza_camara() 
{
    if(!camara_sigue_personaje) 
        return;
    
    //Hace mas o menos una interpolacion lineal suave
    camara_global.pos_x += (camara_global.objetivo_x - camara_global.pos_x) * camara_global.suavidad;
    camara_global.pos_y += (camara_global.objetivo_y - camara_global.pos_y) * camara_global.suavidad;
    camara_global.zoom += (camara_global.objetivo_zoom - camara_global.zoom) * camara_global.suavidad;
}

void aplica_camara() 
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    double ancho_ventana = 1200.0;
    double alto_ventana = 1000.0;
    
    //Calcular los limites de la vista con zoom
    double mitad_ancho = (ancho_ventana / 2.0) / camara_global.zoom;
    double mitad_alto = (alto_ventana / 2.0) / camara_global.zoom;
    
    //Centra la camara en la posicion del personaje
    double izquierda = camara_global.pos_x - mitad_ancho;
    double derecha = camara_global.pos_x + mitad_ancho;
    double abajo = camara_global.pos_y - mitad_alto;
    double arriba = camara_global.pos_y + mitad_alto;
    
    gluOrtho2D(izquierda, derecha, abajo, arriba);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void alterna_seguimiento_camara()
{
    camara_sigue_personaje = !camara_sigue_personaje;

    printf("Seguimiento de camara: %s\n", camara_sigue_personaje ? "ACTIVADO" : "DESACTIVADO");
    
    glutSetWindow(ventana_controles);
    glutPostRedisplay();
}

void pausa()
{
    en_pausa = !en_pausa;
    
    if(en_pausa) 
    {
        //Pausa todos los audios activos
        if(frame_actual != NULL && frame_actual->arbol_jerarquia != NULL)
        {
            //Busca todos los dialogos activos en la jerarquia
            NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame_actual->arbol_jerarquia);

            if(nodo_atomix != NULL)
            {
                Personaje *atomix = (Personaje*)nodo_atomix->dato;

                if(atomix->dialogo != NULL && atomix->dialogo->activo && atomix->dialogo->audio != NULL && atomix->dialogo->audio->reproduciendo)
                {
                    detiene_audio(atomix->dialogo->audio);
                    atomix->dialogo->audio_pausado = true;
                    puts("Audio pausado");
                }
            }
        }

        puts("=== PAUSA ===");
    }

    else 
    {
        //Reanuda audios que estaban pausados
        if(frame_actual != NULL && frame_actual->arbol_jerarquia != NULL)
        {
            NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame_actual->arbol_jerarquia);

            if(nodo_atomix != NULL)
            {
                Personaje *atomix = (Personaje*)nodo_atomix->dato;

                if(atomix->dialogo != NULL && atomix->dialogo->activo && atomix->dialogo->audio != NULL && atomix->dialogo->audio_pausado)
                {
                    reproduce_audio(atomix->dialogo->audio);
                    atomix->dialogo->audio_pausado = false;
                    puts("Audio reanudado");
                }
            }
        }

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
        int frames_retroceder = 60; //retrocede 2s (o sea 60frames ya que esta en 30fps)
        int frames_retrocedidos = 0;
        
        Escena *escena_anterior = escena_actual;
        Frame *frame_anterior = frame_actual;

        //Guarda el estado actual antes de retroceder
        Frame *frame_actual_guardado = frame_actual;
        Escena *escena_actual_guardada = escena_actual;
        
        //Retrocede 2s o hasta que se acabe la pila
        while(pila_deshacer->tope != NULL && frames_retrocedidos < frames_retroceder) 
        {
            frame_anterior = pop_pila_frame(pila_deshacer, &escena_anterior);
            frames_retrocedidos++;
        }
        
        if(frames_retrocedidos > 0) 
        {
            frame_actual = frame_anterior;
            escena_actual = escena_anterior;
            tiempo_acumulado = 0.0;
            
            //Deteiene cualquier audio actual antes de retroceder
            if(frame_actual_guardado != NULL && frame_actual_guardado->arbol_jerarquia != NULL)
            {
                NodoJerarquia *nodo_atomix_actual = busca_mr_atomix_en_arbol(frame_actual_guardado->arbol_jerarquia);

                if(nodo_atomix_actual != NULL)
                {
                    Personaje *atomix_actual = (Personaje*)nodo_atomix_actual->dato;

                    if(atomix_actual->dialogo != NULL && atomix_actual->dialogo->audio != NULL)
                        detiene_audio(atomix_actual->dialogo->audio);
                }
            }
            
            //Reactiva el dialogo en el frame al que se retrocedio
            if(frame_actual != NULL && frame_actual->arbol_jerarquia != NULL)
            {
                NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame_actual->arbol_jerarquia);

                if(nodo_atomix != NULL)
                {
                    Personaje *atomix = (Personaje*)nodo_atomix->dato;
                    if(atomix->dialogo != NULL)
                    {
                        //Reinicia el tiempo del dialogo
                        atomix->dialogo->tiempo_mostrado = 0.0f;
                        atomix->dialogo->activo = true;
                        atomix->dialogo->audio_pausado = false;
                        
                        //Reproduce el audio desde el principio
                        if(en_pausa == false && atomix->dialogo->audio != NULL && atomix->dialogo->audio->cargado)
                        {
                            //Configura para loop mientras se muestra
                            set_audio_loop(atomix->dialogo->audio, true);
                            reproduce_audio(atomix->dialogo->audio);
                        }
                    }
                }
            }
            
            printf("Retrocedido %d frames\n", frames_retrocedidos);
        }
    }
    else 
    {
        puts("No hay frames anteriores para retroceder");
    }

}

void reinicia_pelicula() 
{
    if(pelicula_global == NULL) 
    {
        puts("Error: No existe una pelicula");
        return;
    }

    //Detiene cualquier audio en reproduccion
    if(frame_actual != NULL && frame_actual->arbol_jerarquia != NULL)
    {
        NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame_actual->arbol_jerarquia);

        if(nodo_atomix != NULL)
        {
            Personaje *atomix = (Personaje*)nodo_atomix->dato;

            if(atomix->dialogo != NULL && atomix->dialogo->audio != NULL)
            {
                detiene_audio(atomix->dialogo->audio);

                if(atomix->dialogo->audio_pausado)
                    atomix->dialogo->audio_pausado = false;
            }
        }
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
        
        puts("Pelicula reiniciada desde el inicio");
        
        //Reinicia dialogo si el frame inicial lo tiene
        if(frame_actual != NULL && frame_actual->arbol_jerarquia != NULL)
        {
            NodoJerarquia *nodo_atomix = busca_mr_atomix_en_arbol(frame_actual->arbol_jerarquia);

            if(nodo_atomix != NULL)
            {
                Personaje *atomix = (Personaje*)nodo_atomix->dato;

                if(atomix->dialogo != NULL)
                {
                    atomix->dialogo->tiempo_mostrado = 0.0f;
                    atomix->dialogo->activo = true;
                    
                    if(atomix->dialogo->audio != NULL && atomix->dialogo->audio->cargado)
                    {
                        set_audio_loop(atomix->dialogo->audio, true);
                        reproduce_audio(atomix->dialogo->audio);
                    }
                }
            }
        }
    } 

    else 
    {
        puts("Advertencia: No hay escenas en la pelicula");
        frame_actual = NULL;
    }
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

    //boton para seguimiento de camara
    botones[3].x = x_inicio + (ancho_boton + espacio) * 3;
    botones[3].y = y_pos;
    botones[3].ancho = ancho_boton;
    botones[3].alto = alto_boton;
    strcpy(botones[3].texto, camara_sigue_personaje ? "Camara ON" : "Camara OFF");
    botones[3].accion = alterna_seguimiento_camara;
    botones[3].hover = false;
    
    //boton para salir
    botones[4].x = x_inicio + (ancho_boton + espacio) * 4;
    botones[4].y = y_pos;
    botones[4].ancho = ancho_boton;
    botones[4].alto = alto_boton;
    strcpy(botones[4].texto, "Salir");
    botones[4].accion = salir;
    botones[4].hover = false;
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

    strcpy(botones[3].texto, camara_sigue_personaje ? "Camara ON" : "Camara OFF");
    
    //Dibuja todos los botones
    for(int i = 0; i < num_botones; i++)
        dibuja_boton(&botones[i]);
    
    //Cantidad de frames
    glColor3f(0.8, 0.8, 0.8);
    glRasterPos2f(550, 70);
    char info[100];

    sprintf(info, "Frames: %d", cuenta_frames_pila(pila_deshacer));

    for(int i = 0; info[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info[i]);
    
    //Escena
    if(escena_actual != NULL) 
    {
        glRasterPos2f(550, 50);
        sprintf(info, "Escena: %s", escena_actual->nombre);

        for(int i = 0; info[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info[i]);
    }
    
    //Estado de ela pelicula
    glRasterPos2f(550, 30);
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
    
    aplica_camara();
    
    if(frame_actual != NULL) 
    {
        renderiza_frame(frame_actual);
        renderiza_dialogos_frame(frame_actual);
    }
    
    //Simbolo de pausa
    if(en_pausa) 
    {
        //Guarda la matriz actual
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, 1200, 0, 1000);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        glDisable(GL_TEXTURE_2D);
        
        //Fondo semi-transparente oscuro
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0, 0.0, 0.0, 0.5);
        glBegin(GL_POLYGON);
        glVertex2f(0, 0);
        glVertex2f(1200, 0);
        glVertex2f(1200, 1000);
        glVertex2f(0, 1000);
        glEnd();
        
        //Simbolo de pausa central
        glColor3f(1.0, 1.0, 1.0);
        
        float centro_x = 600.0;
        float centro_y = 500.0;
        float ancho_barra = 20.0;
        float alto_barra = 100.0;
        float separacion = 30.0;
        
        //Barra izquierda
        glBegin(GL_POLYGON);
        glVertex2f(centro_x - separacion - ancho_barra, centro_y - alto_barra/2);
        glVertex2f(centro_x - separacion, centro_y - alto_barra/2);
        glVertex2f(centro_x - separacion, centro_y + alto_barra/2);
        glVertex2f(centro_x - separacion - ancho_barra, centro_y + alto_barra/2);
        glEnd();
        
        //Barra derecha
        glBegin(GL_POLYGON);
        glVertex2f(centro_x + separacion, centro_y - alto_barra/2);
        glVertex2f(centro_x + separacion + ancho_barra, centro_y - alto_barra/2);
        glVertex2f(centro_x + separacion + ancho_barra, centro_y + alto_barra/2);
        glVertex2f(centro_x + separacion, centro_y + alto_barra/2);
        glEnd();
        
        //Texto de pausado
        glColor3f(1.0, 1.0, 1.0);
        glRasterPos2f(centro_x - 35, centro_y - alto_barra/2 - 30);
        char *texto_pausa = "PAUSA";
        for(int i = 0; texto_pausa[i] != '\0'; i++) 
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, texto_pausa[i]);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
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
        
        actualiza_objetivo_camara(frame_actual);
        
        actualiza_camara();
        
        actualiza_dialogos_frame(frame_actual, tiempo);
        
        if(tiempo_acumulado >= frame_actual->tiempo_duracion) 
        {
            tiempo_acumulado = 0.0;
            push_pila_frame(pila_deshacer, frame_actual, escena_actual);
            frame_actual = frame_actual->sig;
            
            if(frame_actual == NULL) 
            {
                escena_actual = escena_actual->sig;
                if(escena_actual != NULL) 
                    frame_actual = escena_actual->primer_frame;
                else 
                    printf("FIN DE LA PELICULA\n");
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

        case 'c':
        case 'C':
            camara_sigue_personaje = !camara_sigue_personaje;
            printf("Seguimiento de camara: %s\n", camara_sigue_personaje ? "ACTIVADO" : "DESACTIVADO");
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

void encola_todas_las_texturas(ColaRecursos *cola) 
{
    //Mr Atomix
    encola_recurso(cola, "Figuras/Texturas/casco.png", 0);
    encola_recurso(cola, "Figuras/Texturas/traje.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/guante.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/pantalon.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/zapato.jpg", 0);
    
    //Escena 1
    encola_recurso(cola, "Figuras/Texturas/pasto.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/cielo.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/balon.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/tronco.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/hoja.jpg", 0);

    //Audio dialogo
    encola_recurso(cola, "Audio/dialogo.mp3", 2);

    //Escena 2
    encola_recurso(cola, "Figuras/Texturas/vena.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/globulo_blanco.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/globulo_rojo.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/plaqueta.jpg", 0);

    //Escena 3
    encola_recurso(cola, "Figuras/Texturas/celula.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/mitocondria.jpg", 0);
    encola_recurso(cola, "Figuras/Texturas/adn.jpg", 0);
}

Audio *busca_audio_en_cola(ColaRecursos *cola, char *ruta) 
{
    NodoRecurso *nodo = busca_recurso(cola, ruta);
    
    if(nodo == NULL || nodo->dato_cargado == NULL) 
    {
        printf("Error: Audio no encontrado en cola: %s\n", ruta);
        return NULL;
    }
    
    return (Audio*)nodo->dato_cargado;
}

Personaje *crea_mr_atomix() 
{
    //Torso (su raiz)
    Punto *rot_torso = crea_punto(11, 0.0, 11.288829237070836, 0, 0, 0);
    Personaje *torso = crea_personaje(1, "torso", rot_torso);
    free(rot_torso);

    Punto *pts_torso[] = 
    {
        crea_punto(1, -0.43780011382007067, 14.055403590225414, 0, 0.0, 0.0),
        crea_punto(2, 0.5507693591938506, 13.982176221854012, 0, 1.0, 0.0),
        crea_punto(3, 1.9222321263005, 12.927863898716467, 0, 1.0, 1.0),
        crea_punto(4, 1.9229840215597918, 12.234625900000523, 0, 0.0, 1.0),
        crea_punto(5, 1.9222321263005, 8.927863898716467, 0, 0.0, 0.5),
        crea_punto(6, 0.9584636256010058, 7.679541370359747, 0, 0.5, 0.5),
        crea_punto(7, -0.9729509212840275, 7.748520461319927, 0, 0.5, 0.0),
        crea_punto(8, -2.0777678736994996, 8.927863898716467, 0, 0.0, 0.0),
        crea_punto(9, -2.0777678736994996, 12.240394234879222, 0, 0.0, 1.0),
        crea_punto(10, -2.0777678736994996, 12.927863898716467, 0, 1.0, 1.0)
    };

    torso->num_puntos = 10;
    torso->puntos_figura = (Punto*)malloc(10 * sizeof(Punto));

    for(int i = 0; i < 10; i++) 
    {
        torso->puntos_figura[i] = *pts_torso[i];
        free(pts_torso[i]);
    }
    
    //Asigna textura al torso
    if(cola_recursos_global != NULL) 
        torso->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Cuello (el cual es un hijo de torso)
    Punto *rot_cuello = crea_punto(14, -0.0639464435353998, 14.524080080243076, 0, 0, 0);
    Personaje *cuello = crea_personaje(2, "cuello", rot_cuello);
    free(rot_cuello);

    Punto *pts_cuello[] = 
    {
        crea_punto(12, -0.40118642963437146, 15.080586747425041, 0, 0.0, 0.0),
        crea_punto(13, 0.5141556750081478, 15.080586747425041, 0, 1.0, 0.0),
        crea_punto(2, 0.5507693591938506, 13.982176221854012, 0, 1.0, 1.0),
        crea_punto(1, -0.43780011382007067, 14.055403590225414, 0, 0.0, 1.0)
    };

    cuello->num_puntos = 4;
    cuello->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        cuello->puntos_figura[i] = *pts_cuello[i];
        free(pts_cuello[i]);
    }
    
    //Asigna textura al cuello
    if(cola_recursos_global != NULL) 
        cuello->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Cabeza (el cual es un hijo de cuello)
    double cab_x = -0.07776787369949978;
    double cab_y = 16.927863898716474;
    Punto *rot_cabeza = crea_punto(15, cab_x, cab_y, 0, 0, 0);
    Personaje *cabeza = crea_personaje(3, "cabeza", rot_cabeza);
    free(rot_cabeza);

    //Genera circulo para la cabeza
    int num_puntos_circulo = 20;
    double radio_cabeza = 1.95;
    cabeza->num_puntos = num_puntos_circulo;
    cabeza->puntos_figura = (Punto*)malloc(num_puntos_circulo * sizeof(Punto));

    for(int i = 0; i < num_puntos_circulo; i++) 
    {
        double angulo = 2.0 * PI * i / num_puntos_circulo;
        cabeza->puntos_figura[i].id = 100 + i;
        cabeza->puntos_figura[i].x = cab_x + radio_cabeza * cos(angulo);
        cabeza->puntos_figura[i].y = cab_y + radio_cabeza * sin(angulo);
        cabeza->puntos_figura[i].z = 0;
        cabeza->puntos_figura[i].u = 0.5 + 0.5 * cos(angulo);
        cabeza->puntos_figura[i].v = 0.5 + 0.5 * sin(angulo);
    }
    
    //Asigna textura a la cabeza
    if(cola_recursos_global != NULL) 
        cabeza->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/casco.png");

    //Brazo izquierdo (el cual es un hijo de torso)
    Punto *rot_brazo_izq = crea_punto(18, -2.0777678736994996, 12.592281658715061, 0, 0, 0);
    Personaje *brazo_izq = crea_personaje(4, "brazo_izquierdo", rot_brazo_izq);
    free(rot_brazo_izq);

    Punto *pts_brazo_izq[] = 
    {
        crea_punto(10, -2.0777678736994996, 12.927863898716467, 0, 1.0, 0.0),
        crea_punto(9, -2.0777678736994996, 12.240394234879222, 0, 1.0, 1.0),
        crea_punto(16, -4.533740818548646, 12.26854522878609, 0, 0.0, 1.0),
        crea_punto(17, -4.5620494136876015, 12.970116970194763, 0, 0.0, 0.0)
    };

    brazo_izq->num_puntos = 4;
    brazo_izq->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        brazo_izq->puntos_figura[i] = *pts_brazo_izq[i];
        free(pts_brazo_izq[i]);
    }
    
    //Asigna textura al brazo izquierdo
    if(cola_recursos_global != NULL) 
        brazo_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Codo izquierdo (el cual es un hijo de brazo izquierdo)
    Punto *rot_codo_izq = crea_punto(21, -4.547483304863437, 12.620530358414843, 0, 0, 0);
    Personaje *codo_izq = crea_personaje(5, "codo_izquierdo", rot_codo_izq);
    free(rot_codo_izq);

    Punto *pts_codo_izq[] = 
    {
        crea_punto(17, -4.5620494136876015, 12.970116970194763, 0, 1.0, 0.0),
        crea_punto(16, -4.533740818548646, 12.26854522878609, 0, 1.0, 1.0),
        crea_punto(19, -6.091490840224733, 12.241811528986597, 0, 0.0, 1.0),
        crea_punto(20, -6.077767873699498, 12.927863898716467, 0, 0.0, 0.0)
    };

    codo_izq->num_puntos = 4;
    codo_izq->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++)
    {
        codo_izq->puntos_figura[i] = *pts_codo_izq[i];
        free(pts_codo_izq[i]);
    }
    
    //Asigna textura al codo izquierdo
    if(cola_recursos_global != NULL) 
        codo_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Mano izquierda (la cual es hija de codo izquierdo)
    double mano_izq_x = -6.53188255196353;
    double mano_izq_y = 12.643092738069981;
    Punto *rot_mano_izq = crea_punto(22, mano_izq_x, mano_izq_y, 0, 0, 0);
    Personaje *mano_izq = crea_personaje(6, "mano_izquierda", rot_mano_izq);
    free(rot_mano_izq);

    double radio_mano = 0.6;
    mano_izq->num_puntos = num_puntos_circulo;
    mano_izq->puntos_figura = (Punto*)malloc(num_puntos_circulo * sizeof(Punto));

    for(int i = 0; i < num_puntos_circulo; i++) 
    {
        double angulo = 2.0 * PI * i / num_puntos_circulo;
        mano_izq->puntos_figura[i].id = 200 + i;
        mano_izq->puntos_figura[i].x = mano_izq_x + radio_mano * cos(angulo);
        mano_izq->puntos_figura[i].y = mano_izq_y + radio_mano * sin(angulo);
        mano_izq->puntos_figura[i].z = 0;
        mano_izq->puntos_figura[i].u = 0.5 + 0.5 * cos(angulo);
        mano_izq->puntos_figura[i].v = 0.5 + 0.5 * sin(angulo);
    }
    
    //Asigna textura a la mano izquierda
    if(cola_recursos_global != NULL) 
        mano_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/guante.jpg");

    //Brazo derecho (el cual es hijo de torso)
    Punto *rot_brazo_der = crea_punto(25, 1.9222321263005, 12.591594581118464, 0, 0, 0);
    Personaje *brazo_der = crea_personaje(7, "brazo_derecho", rot_brazo_der);
    free(rot_brazo_der);

    Punto *pts_brazo_der[] = 
    {
        crea_punto(3, 1.9222321263005, 12.927863898716467, 0, 1.0, 0.0),
        crea_punto(4, 1.9229840215597918, 12.234625900000523, 0, 1.0, 1.0),
        crea_punto(23, 4.523755841133346, 12.19382947930133, 0, 0.0, 1.0),
        crea_punto(24, 4.482959420434158, 12.917965946712009, 0, 0.0, 0.0)
    };

    brazo_der->num_puntos = 4;
    brazo_der->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        brazo_der->puntos_figura[i] = *pts_brazo_der[i];
        free(pts_brazo_der[i]);
    }
    
    //Asigna textura al brazo derecho
    if(cola_recursos_global != NULL) 
        brazo_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Codo derecho (el cual es hijo de brazo derecho)
    Punto *rot_codo_der = crea_punto(28, 4.503357630783748, 12.540599055244472, 0, 0, 0);
    Personaje *codo_der = crea_personaje(8, "codo_derecho", rot_codo_der);
    free(rot_codo_der);

    Punto *pts_codo_der[] = 
    {
        crea_punto(24, 4.482959420434158, 12.917965946712009, 0, 1.0, 0.0),
        crea_punto(23, 4.523755841133346, 12.19382947930133, 0, 1.0, 1.0),
        crea_punto(26, 6.41879752117055, 12.22246360524533, 0, 0.0, 1.0),
        crea_punto(27, 6.395461967885172, 12.922530203806797, 0, 0.0, 0.0)
    };

    codo_der->num_puntos = 4;
    codo_der->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for (int i = 0; i < 4; i++) 
    {
        codo_der->puntos_figura[i] = *pts_codo_der[i];
        free(pts_codo_der[i]);
    }
    
    //Asigna textura al codo derecho
    if(cola_recursos_global != NULL) 
        codo_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/traje.jpg");

    //Mano derecha (la cual es hija de codo derecho)
    double mano_der_x = 6.792166373736666;
    double mano_der_y = 12.584164681168756;
    Punto *rot_mano_der = crea_punto(29, mano_der_x, mano_der_y, 0, 0, 0);
    Personaje *mano_der = crea_personaje(9, "mano_derecha", rot_mano_der);
    free(rot_mano_der);

    mano_der->num_puntos = num_puntos_circulo;
    mano_der->puntos_figura = (Punto*)malloc(num_puntos_circulo * sizeof(Punto));

    for(int i = 0; i < num_puntos_circulo; i++)
    {
        double angulo = 2.0 * PI * i / num_puntos_circulo;
        mano_der->puntos_figura[i].id = 300 + i;
        mano_der->puntos_figura[i].x = mano_der_x + radio_mano * cos(angulo);
        mano_der->puntos_figura[i].y = mano_der_y + radio_mano * sin(angulo);
        mano_der->puntos_figura[i].z = 0;
        mano_der->puntos_figura[i].u = 0.5 + 0.5 * cos(angulo);
        mano_der->puntos_figura[i].v = 0.5 + 0.5 * sin(angulo);
    }
    
    //Asigna textura a la mano derecha
    if(cola_recursos_global != NULL) 
        mano_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/guante.jpg");

    //Pierna izquierda (la cual es hija de torso)
    Punto *rot_pierna_izq = crea_punto(33, -1.5321251454883669, 8.345414318021627, 0, 0, 0);
    Personaje *pierna_izq = crea_personaje(10, "pierna_izquierda", rot_pierna_izq);
    free(rot_pierna_izq);

    Punto *pts_pierna_izq[] = 
    {
        crea_punto(8, -2.0777678736994996, 8.927863898716467, 0, 0.0, 0.0),
        crea_punto(30, -2.0777678736994996, 5.0, 0, 0.0, 1.0),
        crea_punto(31, -0.7551282629462819, 5.043136692795641, 0, 1.0, 1.0),
        crea_punto(32, -0.27845504265492366, 7.727033019779604, 0, 1.0, 0.5),
        crea_punto(7, -0.9729509212840275, 7.748520461319927, 0, 0.5, 0.5)
    };

    pierna_izq->num_puntos = 5;
    pierna_izq->puntos_figura = (Punto*)malloc(5 * sizeof(Punto));

    for(int i = 0; i < 5; i++) 
    {
        pierna_izq->puntos_figura[i] = *pts_pierna_izq[i];
        free(pts_pierna_izq[i]);
    }
    
    //Asigna textura a la pierna izquierda
    if(cola_recursos_global != NULL) 
        pierna_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/pantalon.jpg");

    //Rodilla izquierda (la cual es hija de pierna izquierda)
    Punto *rot_rodilla_izq = crea_punto(36, -1.4808743925708157, 5.012202476226791, 0, 0, 0);
    Personaje *rodilla_izq = crea_personaje(11, "rodilla_izquierda", rot_rodilla_izq);
    free(rot_rodilla_izq);

    Punto *pts_rodilla_izq[] = 
    {
        crea_punto(30, -2.0777678736994996, 5.0, 0, 0.0, 0.0),
        crea_punto(31, -0.7551282629462819, 5.043136692795641, 0, 1.0, 0.0),
        crea_punto(34, -1.1187852996750036, 2.995575085366731, 0, 1.0, 1.0),
        crea_punto(35, -2.0777678736994996, 2.927863898716467, 0, 0.0, 1.0)
    };

    rodilla_izq->num_puntos = 4;
    rodilla_izq->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        rodilla_izq->puntos_figura[i] = *pts_rodilla_izq[i];
        free(pts_rodilla_izq[i]);
    }
    
    //Asigna textura a la rodilla izquierda
    if(cola_recursos_global != NULL) 
        rodilla_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/pantalon.jpg");

    //Pie izquierdo (el cual es hijo de rodilla izquierda)
    Punto *rot_pie_izq = crea_punto(39, -1.89092548863924, 2.4695700703988432, 0, 0, 0);
    Personaje *pie_izq = crea_personaje(12, "pie_izquierdo", rot_pie_izq);
    free(rot_pie_izq);

    Punto *pts_pie_izq[] = 
    {
        crea_punto(34, -1.1187852996750036, 2.995575085366731, 0, 0.0, 0.0),
        crea_punto(35, -2.0777678736994996, 2.927863898716467, 0, 1.0, 0.0),
        crea_punto(37, -2.6884073677300786, 2.353724639400469, 0, 1.0, 1.0),
        crea_punto(38, -1.631210196284186, 1.8985425239168174, 0, 0.0, 1.0)
    };

    pie_izq->num_puntos = 4;
    pie_izq->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        pie_izq->puntos_figura[i] = *pts_pie_izq[i];
        free(pts_pie_izq[i]);
    }
    
    //Asigna textura al pie izquierdo
    if(cola_recursos_global != NULL) 
        pie_izq->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/zapato.jpg");

    //Pierna derecha (la cual es hija de torso)
    Punto *rot_pierna_der = crea_punto(43, 1.3401846801530235, 8.17396611198548, 0, 0, 0);
    Personaje *pierna_der = crea_personaje(13, "pierna_derecha", rot_pierna_der);
    free(rot_pierna_der);

    Punto *pts_pierna_der[] = 
    {
        crea_punto(5, 1.9222321263005, 8.927863898716467, 0, 0.0, 0.0),
        crea_punto(40, 2.101689927946859, 5.0046515582985185, 0, 0.0, 1.0),
        crea_punto(41, 0.690626408623049, 4.996745553846009, 0, 1.0, 1.0),
        crea_punto(42, 0.030053601124750435, 7.702592052454861, 0, 1.0, 0.5),
        crea_punto(6, 0.9584636256010058, 7.679541370359747, 0, 0.5, 0.5)
    };

    pierna_der->num_puntos = 5;
    pierna_der->puntos_figura = (Punto*)malloc(5 * sizeof(Punto));

    for(int i = 0; i < 5; i++) 
    {
        pierna_der->puntos_figura[i] = *pts_pierna_der[i];
        free(pts_pierna_der[i]);
    }
    
    //Asigna textura a la pierna derecha
    if(cola_recursos_global != NULL) 
        pierna_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/pantalon.jpg");

    //Rodilla derecha (la cual es hija de pierna derecha)
    Punto *rot_rodilla_der = crea_punto(46, 1.5382199240784127, 4.995241272200672, 0, 0, 0);
    Personaje *rodilla_der = crea_personaje(14, "rodilla_derecha", rot_rodilla_der);
    free(rot_rodilla_der);

    Punto *pts_rodilla_der[] = 
    {
        crea_punto(40, 2.101689927946859, 5.0046515582985185, 0, 0.0, 0.0),
        crea_punto(41, 0.690626408623049, 4.996745553846009, 0, 1.0, 0.0),
        crea_punto(44, 1.1852246849682402, 2.9660364958200223, 0, 1.0, 1.0),
        crea_punto(45, 2.189536729556298, 3.0841908540068665, 0, 0.0, 1.0)
    };

    rodilla_der->num_puntos = 4;
    rodilla_der->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        rodilla_der->puntos_figura[i] = *pts_rodilla_der[i];
        free(pts_rodilla_der[i]);
    }
    
    //Asigna textura a la rodilla derecha
    if(cola_recursos_global != NULL) 
        rodilla_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/pantalon.jpg");

    //Pie derecho (el cual es hijo de pierna derecha)
    Punto *rot_pie_der = crea_punto(49, 1.9237479839831515, 2.5337938747015727, 0, 0, 0);
    Personaje *pie_der = crea_personaje(15, "pie_derecho", rot_pie_der);
    free(rot_pie_der);

    Punto *pts_pie_der[] = 
    {
        crea_punto(44, 1.1852246849682402, 2.9660364958200223, 0, 0.0, 0.0),
        crea_punto(45, 2.189536729556298, 3.0841908540068665, 0, 1.0, 0.0),
        crea_punto(47, 2.9352942526001597, 2.544607462022646, 0, 1.0, 1.0),
        crea_punto(48, 1.3935483775748987, 1.9572757001082564, 0, 0.0, 1.0)
    };

    pie_der->num_puntos = 4;
    pie_der->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        pie_der->puntos_figura[i] = *pts_pie_der[i];
        free(pts_pie_der[i]);
    }
    
    //Asigna textura al pie derecho
    if(cola_recursos_global != NULL) 
    {
        pie_der->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/zapato.jpg");
    }

    //Jerarquia

    //Torso -> Cuello -> Cabeza
    agrega_hijo_personaje(torso, cuello);
    agrega_hijo_personaje(cuello, cabeza);
    
    //Torso -> Brazo_izq -> Codo_izq -> Mano_izq
    agrega_hijo_personaje(torso, brazo_izq);
    agrega_hijo_personaje(brazo_izq, codo_izq);
    agrega_hijo_personaje(codo_izq, mano_izq);
    
    //Torso -> Brazo_der -> Codo_der -> Mano_der
    agrega_hijo_personaje(torso, brazo_der);
    agrega_hijo_personaje(brazo_der, codo_der);
    agrega_hijo_personaje(codo_der, mano_der);
    
    //Torso -> Pierna_izq -> Rodilla_izq -> Pie_izq
    agrega_hijo_personaje(torso, pierna_izq);
    agrega_hijo_personaje(pierna_izq, rodilla_izq);
    agrega_hijo_personaje(rodilla_izq, pie_izq);
    
    //Torso -> Pierna_der -> Rodilla_der -> Pie_der
    agrega_hijo_personaje(torso, pierna_der);
    agrega_hijo_personaje(pierna_der, rodilla_der);
    agrega_hijo_personaje(rodilla_der, pie_der);

    //Convierte las coordenadas a relativas
    convierte_absolutas_a_relativas_personaje(torso, 0.0, 0.0);

    return torso;
}

Personaje *crea_piso() 
{
    //Piso es un rectangulo que cubre toda la pantalla
    Punto *rot_piso = crea_punto(800, 1500.0, 100.0, 0, 0, 0);
    Personaje *piso = crea_personaje(1000, "piso", rot_piso);
    free(rot_piso);

    Punto *pts_piso[] = 
    {
        crea_punto(801, 0.0, 0.0, 0, 0.0, 0.0),
        crea_punto(802, 2000.0, 0.0, 0, 10.0, 0.0),
        crea_punto(803, 2000.0, 200.0, 0, 10.0, 2.0),
        crea_punto(804, 0.0, 200.0, 0, 0.0, 2.0)
    };
    
    piso->num_puntos = 4;
    piso->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));
    
    for(int i = 0; i < 4; i++) 
    {
        piso->puntos_figura[i] = *pts_piso[i];
        free(pts_piso[i]);
    }
    
    //Convierte coordenadas a relativas
    convierte_absolutas_a_relativas_personaje(piso, 0.0, 0.0);
    
    // Asigna textura al piso
    if(cola_recursos_global != NULL) 
        piso->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/pasto.jpg");
    
    return piso;
}

Personaje *crea_pino() 
{
    //Tronco (el cual es la raiz)
    Punto *rot_tronco = crea_punto(5, 7.9487052932454, 1.608857500911, 0, 0, 0);
    Personaje *tronco = crea_personaje(16, "tronco_pino", rot_tronco);
    free(rot_tronco);

    Punto *pts_tronco[] = 
    {
        crea_punto(1, 7.0, 0.0, 0, 0.0, 0.0),
        crea_punto(2, 9.0, 0.0, 0, 1.0, 0.0),
        crea_punto(3, 9.0, 3.0, 0, 1.0, 1.0),
        crea_punto(4, 7.0, 3.0, 0, 0.0, 1.0)
    };

    tronco->num_puntos = 4;
    tronco->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        tronco->puntos_figura[i] = *pts_tronco[i];
        free(pts_tronco[i]);
    }
    
    //Asigna textura al tronco
    if(cola_recursos_global != NULL) 
        tronco->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/tronco.jpg");

    //hoja1 (el cual es hijo de tronco)
    Punto *rot_hoja1 = crea_punto(10, 7.9978791971979, 3.7724478420487, 0, 0, 0);
    Personaje *hoja1 = crea_personaje(17, "hoja1_pino", rot_hoja1);
    free(rot_hoja1);

    Punto *pts_hoja1[] = 
    {
        crea_punto(3, 9.0, 3.0, 0, 0.5, 0.0),
        crea_punto(4, 7.0, 3.0, 0, 0.0, 0.0),
        crea_punto(6, 3.054227098302812, 2.991150583326896, 0, 0.0, 1.0),
        crea_punto(7, 4.84, 4.439074557675965, 0, 0.3, 1.2),
        crea_punto(8, 11.16260135465759, 4.439074557675965, 0, 0.7, 1.2),
        crea_punto(9, 12.513997064050054, 2.9187543846094424, 0, 1.0, 1.0)
    };

    hoja1->num_puntos = 6;
    hoja1->puntos_figura = (Punto*)malloc(6 * sizeof(Punto));

    for(int i = 0; i < 6; i++) 
    {
        hoja1->puntos_figura[i] = *pts_hoja1[i];
        free(pts_hoja1[i]);
    }
    
    //Asigna textura a la hoja1
    if(cola_recursos_global != NULL) 
        hoja1->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/hoja.jpg");

    //hoja2 (el cual es hijo de hoja1)
    Punto *rot_hoja2 = crea_punto(13, 7.9978791971979, 5.2403785484755, 0, 0, 0);
    Personaje *hoja2 = crea_personaje(18, "hoja2_pino", rot_hoja2);
    free(rot_hoja2);

    Punto *pts_hoja2[] = 
    {
        crea_punto(7, 4.84, 4.439074557675965, 0, 0.0, 0.0),
        crea_punto(8, 11.16260135465759, 4.439074557675965, 0, 1.0, 0.0),
        crea_punto(11, 13.237959051224585, 6.007658863220787, 0, 1.2, 0.8),
        crea_punto(12, 2.8129064359113007, 5.983526796981636, 0, -0.2, 0.8)
    };

    hoja2->num_puntos = 4;
    hoja2->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        hoja2->puntos_figura[i] = *pts_hoja2[i];
        free(pts_hoja2[i]);
    }
    
    //Asigna textura a la hoja2
    if(cola_recursos_global != NULL) 
        hoja2->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/hoja.jpg");

    //hoja3 (el cual es hijo de hoja2)
    Punto *rot_hoja3 = crea_punto(18, 8.0, 7.0, 0, 0, 0);
    Personaje *hoja3 = crea_personaje(19, "hoja3_pino", rot_hoja3);
    free(rot_hoja3);

    Punto *pts_hoja3[] = 
    {
        crea_punto(14, 4.841708898957756, 6.0419402794746135, 0, 0.0, 0.0),
        crea_punto(15, 11.306789987074035, 6.017358222105351, 0, 1.0, 0.0),
        crea_punto(16, 12.85545960133763, 7.910176639538633, 0, 1.2, 1.0),
        crea_punto(17, 2.776816079939629, 8.008504869015686, 0, -0.2, 1.0)
    };

    hoja3->num_puntos = 4;
    hoja3->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        hoja3->puntos_figura[i] = *pts_hoja3[i];
        free(pts_hoja3[i]);
    }
    
    //Asigna textura a la hoja3
    if(cola_recursos_global != NULL) 
        hoja3->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/hoja.jpg");

    //hoja4 (el cual es hijo de hoja3)
    Punto *rot_hoja4 = crea_punto(25, 8.0, 9.0, 0, 0, 0);
    Personaje *hoja4 = crea_personaje(21, "hoja4_pino", rot_hoja4);
    free(rot_hoja4);

    Punto *pts_hoja4[] = 
    {
        crea_punto(19, 6.0, 8.0, 0, 0.0, 0.0),
        crea_punto(20, 10.0, 8.0, 0, 1.0, 0.0),
        crea_punto(21, 12.00275211022515, 8.786610241381782, 0, 1.2, 0.3),
        crea_punto(22, 10.0, 10.0, 0, 1.0, 1.0),
        crea_punto(23, 6.0, 10.0, 0, 0.0, 1.0),
        crea_punto(24, 4.001009222087615, 8.8121749151458, 0, -0.2, 0.3)
    };

    hoja4->num_puntos = 6;
    hoja4->puntos_figura = (Punto*)malloc(6 * sizeof(Punto));

    for(int i = 0; i < 6; i++) 
    {
        hoja4->puntos_figura[i] = *pts_hoja4[i];
        free(pts_hoja4[i]);
    }
    
    //Asigna textura a la hoja4
    if(cola_recursos_global != NULL) 
        hoja4->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/hoja.jpg");

    //copa (el cual es hijo de hoja4)
    Punto *rot_copa = crea_punto(27, 8.0, 11.0, 0, 0, 0);
    Personaje *copa = crea_personaje(22, "copa_pino", rot_copa);
    free(rot_copa);

    Punto *pts_copa[] = 
    {
        crea_punto(22, 10.0, 10.0, 0, 1.0, 0.0),
        crea_punto(23, 6.0, 10.0, 0, 0.0, 0.0),
        crea_punto(26, 8.0, 12.0, 0, 0.5, 1.0)
    };

    copa->num_puntos = 3;
    copa->puntos_figura = (Punto*)malloc(3 * sizeof(Punto));

    for(int i = 0; i < 3; i++) 
    {
        copa->puntos_figura[i] = *pts_copa[i];
        free(pts_copa[i]);
    }
    
    //Asigna textura a la copa
    if(cola_recursos_global != NULL) 
        copa->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/hoja.jpg");

    //Jerarquia
    agrega_hijo_personaje(tronco, hoja1);
    agrega_hijo_personaje(hoja1, hoja2);
    agrega_hijo_personaje(hoja2, hoja3);
    agrega_hijo_personaje(hoja3, hoja4);
    agrega_hijo_personaje(hoja4, copa);

    convierte_absolutas_a_relativas_personaje(tronco, 0.0, 0.0);

    return tronco;
}

Personaje *crea_balon() 
{
    double centro_x = 5.0;
    double centro_y = 5.0;
    
    Punto *rot_balon = crea_punto(50, centro_x, centro_y, 0, 0, 0);
    Personaje *balon = crea_personaje(20, "balon", rot_balon);
    free(rot_balon);

    int num_puntos_circulo = 20;
    double radio_balon = 1.5;

    balon->num_puntos = num_puntos_circulo;
    balon->puntos_figura = (Punto*)malloc(num_puntos_circulo * sizeof(Punto));

    for(int i = 0; i < num_puntos_circulo; i++) 
    {
        double angulo = 2.0 * PI * i / num_puntos_circulo;
        balon->puntos_figura[i].id = 500 + i;
        balon->puntos_figura[i].x = centro_x + radio_balon * cos(angulo);
        balon->puntos_figura[i].y = centro_y + radio_balon * sin(angulo);
        balon->puntos_figura[i].z = 0;
        balon->puntos_figura[i].u = 0.5 + 0.5 * cos(angulo);
        balon->puntos_figura[i].v = 0.5 + 0.5 * sin(angulo);
    }
    
    convierte_absolutas_a_relativas_personaje(balon, 0.0, 0.0);
    
    //Asigna textura al balon
    if(cola_recursos_global != NULL) 
        balon->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/balon.jpg");
    
    return balon;
}

Personaje *crea_fondo() 
{
    //Fondo es un rectangulo grande que cubre toda las escena
    Punto *rot_fondo = crea_punto(900, 0.0, 0.0, 0, 0, 0);
    Personaje *fondo = crea_personaje(3000, "fondo", rot_fondo);
    free(rot_fondo);

    Punto *pts_fondo[] = 
    {
        crea_punto(901, -1000.0, -500.0, 0, 0.0, 0.0),
        crea_punto(902, 3000.0, -500.0, 0, 4.0, 0.0),
        crea_punto(903, 3000.0, 1500.0, 0, 4.0, 2.0),
        crea_punto(904, -1000.0, 1500.0, 0, 0.0, 2.0)
    };
    
    fondo->num_puntos = 4;
    fondo->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));
    
    for(int i = 0; i < 4; i++) 
    {
        fondo->puntos_figura[i] = *pts_fondo[i];
        free(pts_fondo[i]);
    }
    
    //Convierte coordenadas a relativas
    convierte_absolutas_a_relativas_personaje(fondo, 0.0, 0.0);
    
    //Asigna textura al fondo
    if(cola_recursos_global != NULL) 
    {
        fondo->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/cielo.jpg");
    }
    
    return fondo;
}

void visualiza_escena1() 
{
    Escena *escena_animacion = crea_escena(1, "Parque");

    Audio *audio_dialogo = busca_audio_en_cola(cola_recursos_global, "Audio/dialogo.mp3");
    
    if(audio_dialogo == NULL) 
    {
        puts("No se puedo cargar el audio");
    }

    //Dia soleado
    //Material para el fondo (cielo)
    float amb_cielo[4] = {0.3, 0.4, 0.8, 1.0};
    float diff_cielo[4] = {0.5, 0.6, 1.0, 1.0};
    float spec_cielo[4] = {0.1, 0.1, 0.2, 1.0};
    Material *mat_cielo = crea_material(amb_cielo, diff_cielo, spec_cielo, 1.0);
    
    //Material para el pasto
    float amb_pasto[4] = {0.1, 0.3, 0.1, 1.0};
    float diff_pasto[4] = {0.2, 0.6, 0.2, 1.0};
    float spec_pasto[4] = {0.05, 0.1, 0.05, 1.0};
    Material *mat_pasto = crea_material(amb_pasto, diff_pasto, spec_pasto, 5.0);
    
    //Material para el tronco del pino
    float amb_tronco[4] = {0.3, 0.2, 0.1, 1.0};
    float diff_tronco[4] = {0.5, 0.35, 0.2, 1.0};
    float spec_tronco[4] = {0.1, 0.08, 0.05, 1.0};
    Material *mat_tronco = crea_material(amb_tronco, diff_tronco, spec_tronco, 10.0);
    
    //Material para las hojas del pino
    float amb_hojas[4] = {0.1, 0.25, 0.1, 1.0};
    float diff_hojas[4] = {0.2, 0.5, 0.2, 1.0};
    float spec_hojas[4] = {0.05, 0.1, 0.05, 1.0};
    Material *mat_hojas = crea_material(amb_hojas, diff_hojas, spec_hojas, 15.0);
    
    //Material para el balon
    float amb_balon[4] = {0.3, 0.3, 0.3, 1.0};
    float diff_balon[4] = {0.7, 0.7, 0.7, 1.0};
    float spec_balon[4] = {0.8, 0.8, 0.8, 1.0};
    Material *mat_balon = crea_material(amb_balon, diff_balon, spec_balon, 60.0);
    
    //Material para Mr. Atomix
    float amb_traje[4] = {0.2, 0.15, 0.1, 1.0};
    float diff_traje[4] = {0.5, 0.4, 0.3, 1.0};
    float spec_traje[4] = {0.2, 0.18, 0.15, 1.0};
    Material *mat_traje = crea_material(amb_traje, diff_traje, spec_traje, 20.0);
    
    //Material para el caso de Mr. Atomix
    float amb_casco[4] = {0.25, 0.25, 0.3, 1.0};
    float diff_casco[4] = {0.6, 0.6, 0.7, 1.0};
    float spec_casco[4] = {0.9, 0.9, 0.9, 1.0};
    Material *mat_casco = crea_material(amb_casco, diff_casco, spec_casco, 80.0);
    
    //Material para los guantes y zapatos de Mr Atomix
    float amb_guantes[4] = {0.15, 0.15, 0.15, 1.0};
    float diff_guantes[4] = {0.4, 0.4, 0.4, 1.0};
    float spec_guantes[4] = {0.3, 0.3, 0.3, 1.0};
    Material *mat_guantes = crea_material(amb_guantes, diff_guantes, spec_guantes, 30.0);
    
    //luz del sol (luz principal direccional)
    float pos_sol[4] = {500.0, 1000.0, 500.0, 0.0};
    float amb_sol[4] = {0.4, 0.4, 0.4, 1.0};
    float diff_sol[4] = {0.9, 0.9, 0.8, 1.0};
    float spec_sol[4] = {0.8, 0.8, 0.7, 1.0};
    Luz *luz_sol = crea_luz(0, pos_sol, amb_sol, diff_sol, spec_sol);
    
    //luz de relleno (luz del cielo)
    float pos_cielo[4] = {0.0, 500.0, 0.0, 1.0};
    float amb_cielo_luz[4] = {0.3, 0.3, 0.4, 1.0};
    float diff_cielo_luz[4] = {0.3, 0.3, 0.5, 1.0};
    float spec_cielo_luz[4] = {0.1, 0.1, 0.2, 1.0};
    Luz *luz_cielo = crea_luz(1, pos_cielo, amb_cielo_luz, diff_cielo_luz, spec_cielo_luz);
    
    //10 segundos a 30fps son aprox 300 frames
    //Cada frame dura 1/30 asi que mas o meenos son 0.0333 segundos
    int num_frames = 300;
    double duracion_frame = 1.0 / 30.0;
    
    for(int f = 0; f < num_frames; f++) 
    {
        double t = (double)f / (num_frames - 1);

        //Fondo (Cielo)
        Personaje *fondo = crea_fondo();
        NodoJerarquia *nodo_fondo = crea_nodo_jerarquia(3000, 1, fondo);
        nodo_fondo->pos_x = 0.0;
        nodo_fondo->pos_y = 0.0;
        nodo_fondo->escala = 1.0;
        asigna_material_personaje(fondo, mat_cielo);
        
        //Piso (Pasto)
        Personaje *piso = crea_piso();
        NodoJerarquia *nodo_piso = crea_nodo_jerarquia(100, 1, piso);
        nodo_piso->pos_x = -400.0;
        nodo_piso->pos_y = 0.0;
        nodo_piso->escala = 1.0;
        asigna_material_personaje(piso, mat_pasto);
        agrega_hijo_jerarquia(nodo_fondo, nodo_piso);
        
        //Pino a la derecha
        Personaje *pino = crea_pino();
        NodoJerarquia *nodo_pino = crea_nodo_jerarquia(16, 1, pino);
        nodo_pino->pos_x = 800.0;
        nodo_pino->pos_y = 145.0;
        nodo_pino->escala = 40.0;
        
        //Asigna materiales a las partes del pino
        Personaje *tronco_pino = busca_parte_personaje(pino, "tronco_pino");

        if(tronco_pino) 
            asigna_material_personaje(tronco_pino, mat_tronco);
        
        //Asigna material de las hojas a todas las hojas
        Personaje *partes_hojas[] = 
        {
            busca_parte_personaje(pino, "hoja1_pino"),
            busca_parte_personaje(pino, "hoja2_pino"),
            busca_parte_personaje(pino, "hoja3_pino"),
            busca_parte_personaje(pino, "hoja4_pino"),
            busca_parte_personaje(pino, "copa_pino")
        };
        
        for(int i = 0; i < 5; i++) 
        {
            if(partes_hojas[i]) 
                asigna_material_personaje(partes_hojas[i], mat_hojas);
        }
        
        agrega_hijo_jerarquia(nodo_piso, nodo_pino);
        
        //Pino a la izquierda
        Personaje *pino2 = clona_personaje(pino);
        NodoJerarquia *nodo_pino2 = crea_nodo_jerarquia(17, 1, pino2);
        nodo_pino2->pos_x = -300.0;
        nodo_pino2->pos_y = 145.0;
        nodo_pino2->escala = 40.0;
        agrega_hijo_jerarquia(nodo_piso, nodo_pino2);
        
        //Balon
        Personaje *balon = crea_balon();
        NodoJerarquia *nodo_balon = crea_nodo_jerarquia(20, 1, balon);
        asigna_material_personaje(balon, mat_balon);
        
        //Movimiento de rebote mas o menos sinusoidal
        double altura_rebote = fabs(sin(t * PI * 4)) * 50.0; ////Hace 4 rebotes


        nodo_balon->pos_x = 200.0 + t * 400.0; //Se mueve de izquierda a derecha
        nodo_balon->pos_y = 145.0 + altura_rebote;
        nodo_balon->escala = 28.0;
        nodo_balon->rot_z = t * 720.0; //Hace 2 rotaciones completas
        agrega_hijo_jerarquia(nodo_piso, nodo_balon);
        
        //Mr. Atomix
        Personaje *mr_atomix = crea_mr_atomix();
        
        //Asigna materiales a las partes de Mr. Atomix
        Personaje *torso = busca_parte_personaje(mr_atomix, "torso");

        if(torso) 
            asigna_material_personaje(torso, mat_traje);
        
        Personaje *cuello = busca_parte_personaje(mr_atomix, "cuello");

        if(cuello) 
            asigna_material_personaje(cuello, mat_traje);

        Personaje *cabeza = busca_parte_personaje(mr_atomix, "cabeza");
        
        if(cabeza) 
            asigna_material_personaje(cabeza, mat_casco);
        
        Personaje *brazo_izq = busca_parte_personaje(mr_atomix, "brazo_izquierdo");

        if(brazo_izq) 
            asigna_material_personaje(brazo_izq, mat_traje);
        
        Personaje *brazo_der = busca_parte_personaje(mr_atomix, "brazo_derecho");

        if(brazo_der) 
            asigna_material_personaje(brazo_der, mat_traje);
        
        Personaje *codo_izq = busca_parte_personaje(mr_atomix, "codo_izquierdo");

        if(codo_izq) 
            asigna_material_personaje(codo_izq, mat_traje);
        
        Personaje *codo_der = busca_parte_personaje(mr_atomix, "codo_derecho");

        if(codo_der)
            asigna_material_personaje(codo_der, mat_traje);
        
        Personaje *mano_izq = busca_parte_personaje(mr_atomix, "mano_izquierda");

        if(mano_izq) 
            asigna_material_personaje(mano_izq, mat_guantes);
        
        Personaje *mano_der = busca_parte_personaje(mr_atomix, "mano_derecha");

        if(mano_der) 
            asigna_material_personaje(mano_der, mat_guantes);
        
        Personaje *pierna_izq = busca_parte_personaje(mr_atomix, "pierna_izquierda");

        if(pierna_izq) 
            asigna_material_personaje(pierna_izq, mat_traje);
        
        Personaje *pierna_der = busca_parte_personaje(mr_atomix, "pierna_derecha");

        if(pierna_der) 
            asigna_material_personaje(pierna_der, mat_traje);
        
        Personaje *rodilla_izq = busca_parte_personaje(mr_atomix, "rodilla_izquierda");

        if(rodilla_izq) 
            asigna_material_personaje(rodilla_izq, mat_traje);
        
        Personaje *rodilla_der = busca_parte_personaje(mr_atomix, "rodilla_derecha");

        if(rodilla_der) 
            asigna_material_personaje(rodilla_der, mat_traje);
        
        Personaje *pie_izq = busca_parte_personaje(mr_atomix, "pie_izquierdo");

        if(pie_izq) 
            asigna_material_personaje(pie_izq, mat_guantes);
        
        Personaje 
            *pie_der = busca_parte_personaje(mr_atomix, "pie_derecho");

        if(pie_der) 
            asigna_material_personaje(pie_der, mat_guantes);
        
        if(f < 240) 
        {
            char *dialogos1[] = 
            {
                "Hola amigos! Soy Mr. Atomix.",
                "Hoy vamos a ver que tan grande",
                "y que tan pequeno es",
                "nuestro universo. Vamos abajo!"
            };
            
            Dialogo *dialogo_frame = crea_dialogo(4, dialogos1, audio_dialogo);

            if(dialogo_frame != NULL) 
                muestra_dialogo(mr_atomix, dialogo_frame);
            
            for(int i = 0; i < 4; i++) 
            {
                strcpy(dialogo_frame->lineas[i], dialogos1[i]);
                dialogo_frame->total_caracteres += strlen(dialogos1[i]);
            }
            
            dialogo_frame->audio = audio_dialogo;
            dialogo_frame->activo = true;
            
            dialogo_frame->tiempo_mostrado = f * duracion_frame;
            
            mr_atomix->dialogo = dialogo_frame;
        } 

        else 
            mr_atomix->dialogo = NULL;

        NodoJerarquia *nodo_atomix = crea_nodo_jerarquia(1, 1, mr_atomix);
        
        //Movimiento horizontal de Mr. Atomix (camina de izquierda a derecha)
        nodo_atomix->pos_x = 200.0 + t * 800.0;
        nodo_atomix->pos_y = 145.0;
        nodo_atomix->escala = 28.0;
        
        //Ciclo de caminar (8 ciclos completos en 10 segundos)
        double ciclo_caminar = sin(t * PI * 16) * 15.0; //Oscilacion de brazos y pierna
        
        //Anima brazos
        Personaje *brazo_izq_anim = busca_parte_personaje(mr_atomix, "brazo_izquierdo");
        Personaje *brazo_der_anim = busca_parte_personaje(mr_atomix, "brazo_derecho");

        if(brazo_izq_anim) 
            brazo_izq_anim->angulo_actual = ciclo_caminar;

        if(brazo_der_anim) 
            brazo_der_anim->angulo_actual = -ciclo_caminar;
        
        //Anima piernas (hace lo opuesto a los brazos)
        Personaje *pierna_izq_anim = busca_parte_personaje(mr_atomix, "pierna_izquierda");
        Personaje *pierna_der_anim = busca_parte_personaje(mr_atomix, "pierna_derecha");

        if(pierna_izq_anim) 
            pierna_izq_anim->angulo_actual = -ciclo_caminar * 0.8;

        if(pierna_der_anim) 
            pierna_der_anim->angulo_actual = ciclo_caminar * 0.8;
        
        //Anima codos
        Personaje *codo_izq_anim = busca_parte_personaje(mr_atomix, "codo_izquierdo");
        Personaje *codo_der_anim = busca_parte_personaje(mr_atomix, "codo_derecho");

        if(codo_izq_anim) 
            codo_izq_anim->angulo_actual = fabs(ciclo_caminar) * 0.5;

        if(codo_der_anim) 
            codo_der_anim->angulo_actual = fabs(ciclo_caminar) * 0.5;
        
        //Anima rodillas
        Personaje *rodilla_izq_anim = busca_parte_personaje(mr_atomix, "rodilla_izquierda");
        Personaje *rodilla_der_anim = busca_parte_personaje(mr_atomix, "rodilla_derecha");

        if(rodilla_izq_anim) 
            rodilla_izq_anim->angulo_actual = fabs(ciclo_caminar) * 0.6;

        if(rodilla_der_anim) 
            rodilla_der_anim->angulo_actual = fabs(ciclo_caminar) * 0.6;
        
        //movimiento de cabeza
        Personaje *cabeza_anim = busca_parte_personaje(mr_atomix, "cabeza");

        if(cabeza_anim) 
            cabeza_anim->angulo_actual = sin(t * PI * 8) * 5.0;
        
        agrega_hijo_jerarquia(nodo_piso, nodo_atomix);
        
        //Agrega luces a la jerarquia de los frame
        //Clona las luces para este frame
        Luz *luz_sol_frame = (Luz*)malloc(sizeof(Luz));
        *luz_sol_frame = *luz_sol;
        
        Luz *luz_cielo_frame = (Luz*)malloc(sizeof(Luz));
        *luz_cielo_frame = *luz_cielo;
        
        //Crea nodos para las luces
        NodoJerarquia *nodo_luz_sol = crea_nodo_jerarquia(5000, 3, luz_sol_frame);
        NodoJerarquia *nodo_luz_cielo = crea_nodo_jerarquia(5001, 3, luz_cielo_frame);
        
        //Posiciona luces
        nodo_luz_sol->pos_x = 500.0 + sin(t * PI) * 200.0;
        nodo_luz_sol->pos_y = 1000.0;
        nodo_luz_sol->pos_z = 500.0;
        
        nodo_luz_cielo->pos_x = 0.0;
        nodo_luz_cielo->pos_y = 500.0;
        nodo_luz_cielo->pos_z = 0.0;
        
        //Agrega luces al fondo
        agrega_hijo_jerarquia(nodo_fondo, nodo_luz_sol);
        agrega_hijo_jerarquia(nodo_fondo, nodo_luz_cielo);
        
        //Cre frame con duracion de 1/30 segundo
        Frame *frame = crea_frame(f + 1, nodo_fondo, duracion_frame);
        agrega_frame_escena(escena_animacion, frame);
    }
    
    //Liberar materiales
    free(mat_cielo);
    free(mat_pasto);
    free(mat_tronco);
    free(mat_hojas);
    free(mat_balon);
    free(mat_traje);
    free(mat_casco);
    free(mat_guantes);
    free(luz_sol);
    free(luz_cielo);
    
    encola_escena(pelicula_global, escena_animacion);
}

Personaje *crea_plaqueta() 
{

    Punto *pts_plaqueta[] = 
    {
        crea_punto(1, 5.7254, 4.6486, 0, 0.7283, 0.9531), 
        crea_punto(2, 5.4, 4.74, 0, 0.5042, 1.0000),
        crea_punto(3, 5.74, 4.2, 0, 0.7386, 0.7231),
        crea_punto(4, 5.06, 3.82, 0, 0.2703, 0.5283),
        crea_punto(5, 4.6672, 4.4065, 0, 0.0000, 0.8290),
        crea_punto(6, 4.7925, 3.7574, 0, 0.0862, 0.5050),
        crea_punto(7, 4.7811, 3.2904, 0, 0.0784, 0.2569),
        crea_punto(8, 5.2253, 3.4840, 0, 0.3839, 0.3561),
        crea_punto(9, 5.6694, 3.7915, 0, 0.6908, 0.5138),
        crea_punto(10, 6.12, 3.5, 0, 1.0000, 0.3643),
        crea_punto(11, 6.0453, 2.7893, 0, 0.9481, 0.0000)
    };
    
    int num_puntos = 11;
    
    Punto *rot_plaqueta = crea_punto(50, 5.4, 3.8, 0, 0, 0); 
    
    Personaje *plaqueta = crea_personaje(1000, "plaqueta", rot_plaqueta); 
    free(rot_plaqueta); 
    
    if(plaqueta == NULL) 
    {
        for(int i = 0; i < num_puntos; i++) free_punto(pts_plaqueta[i]);
        return NULL;
    }

    plaqueta->num_puntos = num_puntos;
    plaqueta->puntos_figura = (Punto*)malloc(num_puntos * sizeof(Punto));
    
    if(plaqueta->puntos_figura == NULL) 
    {
        for(int i = 0; i < num_puntos; i++) free_punto(pts_plaqueta[i]);
        free_personaje(plaqueta); 
        return NULL;
    }

    for(int i = 0; i < num_puntos; i++) 
    {
        plaqueta->puntos_figura[i] = *pts_plaqueta[i];
        free_punto(pts_plaqueta[i]); 
    }

    if(cola_recursos_global != NULL)
        plaqueta->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/plaqueta.jpg");

    return plaqueta;
}

Personaje *crea_globulo_rojo()
{
    int num_puntos = 20;
    Punto *pts_globulo[20];
    double angle;
    double center_x = 3.72;
    double center_y = 4.6;
    double center_z = 0;

    Punto *rot_globulo = crea_punto(1, center_x, center_y, center_z, 0, 0); 

    Personaje *globulo = crea_personaje(2000, "globulo_rojo", rot_globulo); 
    free(rot_globulo); 
    
    if(globulo == NULL) 
    {
        return NULL;
    }

    for (int i = 0; i < 20; i++)
    {
        angle = 2.0 * PI * (double)i / (double)20; 

        double x = center_x + 0.25 * cos(angle);
        double y = center_y + 0.25 * sin(angle);

        double u = (double)i / (double)20;
        double v = 0.5;

        pts_globulo[i] = crea_punto(i + 2, x, y, center_z, u, v);
    }
    
    globulo->num_puntos = num_puntos;
    globulo->puntos_figura = (Punto*)malloc(num_puntos * sizeof(Punto));
    
    if(globulo->puntos_figura == NULL) 
    {
        for(int j = 0; j < num_puntos; j++) 
            free_punto(pts_globulo[j]);

        free_personaje(globulo); 
        return NULL;
    }

    for(int i = 0; i < num_puntos; i++) 
    {
        globulo->puntos_figura[i] = *pts_globulo[i];
        free_punto(pts_globulo[i]); 
    }

    if(cola_recursos_global != NULL)
        globulo->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/globulo_rojo.jpg");

    return globulo;
}

Personaje *crea_globulo_blanco()
{
    int num_puntos = 8;
    Punto *pts_globulo[8];
    double angle;
    double center_x = 7.0;
    double center_y = 5.0;
    double center_z = 0; 

    Punto *rot_globulo = crea_punto(1, center_x, center_y, center_z, 0, 0); 

    Personaje *globulo = crea_personaje(3000, "globulo_blanco", rot_globulo); 
    free(rot_globulo); 
    
    if(globulo == NULL) 
    {
        return NULL;
    }

    for(int i = 0; i < 8; i++)
    {
        angle = 2.0 * PI * (double)i / (double)8; 
        
        double x = center_x + 0.35 * cos(angle);
        double y = center_y + 0.35 * sin(angle);
        
        double u = (double)i / (double)8;
        double v = 0.5;

        pts_globulo[i] = crea_punto(i + 2, x, y, center_z, u, v);
    }
    
    globulo->num_puntos = num_puntos;
    globulo->puntos_figura = (Punto*)malloc(num_puntos * sizeof(Punto));
    
    if(globulo->puntos_figura == NULL) 
    {
        for(int j = 0; j < num_puntos; j++) 
            free_punto(pts_globulo[j]);

        free_personaje(globulo); 
        return NULL;
    }

    for(int i = 0; i < num_puntos; i++) 
    {
        globulo->puntos_figura[i] = *pts_globulo[i];
        free_punto(pts_globulo[i]); 
    }

    if(cola_recursos_global != NULL)
        globulo->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/globulo_blanco.jpg");

    return globulo;
}

Personaje *crea_vena() 
{
    //Vena es un rectangulo grande que cubre toda las escena
    Punto *rot_vena = crea_punto(900, 0.0, 0.0, 0, 0, 0);
    Personaje *vena = crea_personaje(3000, "vena", rot_vena);
    free(rot_vena);

    Punto *pts_vena[] = 
    {
        crea_punto(901, -1000.0, -500.0, 0, 0.0, 0.0),
        crea_punto(902, 3000.0, -500.0, 0, 4.0, 0.0),
        crea_punto(903, 3000.0, 1500.0, 0, 4.0, 2.0),
        crea_punto(904, -1000.0, 1500.0, 0, 0.0, 2.0)
    };
    
    vena->num_puntos = 4;
    vena->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));
    
    for(int i = 0; i < 4; i++) 
    {
        vena->puntos_figura[i] = *pts_vena[i];
        free(pts_vena[i]);
    }
    
    //Convierte coordenadas a relativas
    convierte_absolutas_a_relativas_personaje(vena, 0.0, 0.0);
    
    //Asigna textura al fondo
    if(cola_recursos_global != NULL) 
    {
        vena->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/vena.jpg");
    }
    
    return vena;
}

void visualiza_escena2() 
{
    Escena *escena_animacion = crea_escena(2, "Dentro del Cuerpo");

    Audio *audio_dialogo = busca_audio_en_cola(cola_recursos_global, "Audio/dialogo.mp3");
    
    if(audio_dialogo == NULL) 
    {
        puts("No se pudo cargar el audio");
    }

    float amb_vena[4] = {0.3, 0.1, 0.1, 1.0};
    float diff_vena[4] = {0.6, 0.2, 0.2, 1.0};
    float spec_vena[4] = {0.2, 0.1, 0.1, 1.0};
    Material *mat_vena = crea_material(amb_vena, diff_vena, spec_vena, 5.0);
    
    float amb_rojo[4] = {0.3, 0.05, 0.05, 1.0};
    float diff_rojo[4] = {0.8, 0.1, 0.1, 1.0};
    float spec_rojo[4] = {0.5, 0.2, 0.2, 1.0};
    Material *mat_globulo_rojo = crea_material(amb_rojo, diff_rojo, spec_rojo, 20.0);
    
    float amb_blanco[4] = {0.3, 0.3, 0.25, 1.0};
    float diff_blanco[4] = {0.9, 0.9, 0.8, 1.0};
    float spec_blanco[4] = {0.6, 0.6, 0.5, 1.0};
    Material *mat_globulo_blanco = crea_material(amb_blanco, diff_blanco, spec_blanco, 30.0);
    
    float amb_plaqueta[4] = {0.3, 0.25, 0.1, 1.0};
    float diff_plaqueta[4] = {0.7, 0.6, 0.3, 1.0};
    float spec_plaqueta[4] = {0.4, 0.35, 0.2, 1.0};
    Material *mat_plaqueta = crea_material(amb_plaqueta, diff_plaqueta, spec_plaqueta, 15.0);

    float amb_traje[4] = {0.2, 0.15, 0.1, 1.0};
    float diff_traje[4] = {0.5, 0.4, 0.3, 1.0};
    float spec_traje[4] = {0.2, 0.18, 0.15, 1.0};
    Material *mat_traje = crea_material(amb_traje, diff_traje, spec_traje, 20.0);
    
    float amb_casco[4] = {0.25, 0.25, 0.3, 1.0};
    float diff_casco[4] = {0.6, 0.6, 0.7, 1.0};
    float spec_casco[4] = {0.9, 0.9, 0.9, 1.0};
    Material *mat_casco = crea_material(amb_casco, diff_casco, spec_casco, 80.0);
    
    float amb_guantes[4] = {0.15, 0.15, 0.15, 1.0};
    float diff_guantes[4] = {0.4, 0.4, 0.4, 1.0};
    float spec_guantes[4] = {0.3, 0.3, 0.3, 1.0};
    Material *mat_guantes = crea_material(amb_guantes, diff_guantes, spec_guantes, 30.0);
    
    float pos_luz1[4] = {500.0, 800.0, 400.0, 0.0};
    float amb_luz1[4] = {0.3, 0.15, 0.15, 1.0};
    float diff_luz1[4] = {0.8, 0.3, 0.3, 1.0};
    float spec_luz1[4] = {0.5, 0.2, 0.2, 1.0};
    Luz *luz_principal = crea_luz(0, pos_luz1, amb_luz1, diff_luz1, spec_luz1);
    
    float pos_luz2[4] = {-300.0, 600.0, 300.0, 1.0};
    float amb_luz2[4] = {0.2, 0.1, 0.1, 1.0};
    float diff_luz2[4] = {0.4, 0.2, 0.2, 1.0};
    float spec_luz2[4] = {0.2, 0.1, 0.1, 1.0};
    Luz *luz_relleno = crea_luz(1, pos_luz2, amb_luz2, diff_luz2, spec_luz2);
    
    int num_frames = 450;
    double duracion_frame = 1.0 / 30.0;
    
    double pos_plaquetas[5][2] = 
    {
        {300.0, 350.0},
        {500.0, 450.0},
        {700.0, 300.0},
        {900.0, 400.0},
        {1100.0, 350.0}
    };
    
    double pos_blancos[5][2] = 
    {
        {250.0, 300.0},
        {450.0, 500.0},
        {650.0, 250.0},
        {850.0, 450.0},
        {1050.0, 300.0}
    };
    
    double pos_rojos[5][2] = 
    {
        {350.0, 400.0},
        {550.0, 350.0},
        {750.0, 450.0},
        {950.0, 300.0},
        {150.0, 350.0}
    };
    
    for(int f = 0; f < num_frames; f++) 
    {
        double t = (double)f / (num_frames - 1);

        //Vena (fondo)
        Personaje *vena = crea_vena();
        NodoJerarquia *nodo_vena = crea_nodo_jerarquia(4000, 1, vena);
        nodo_vena->pos_x = 0.0;
        nodo_vena->pos_y = 0.0;
        nodo_vena->escala = 1.0;
        asigna_material_personaje(vena, mat_vena);
        
        //Crea y posicionar 25 plaquetas
        for(int i = 0; i < 25; i++) 
        {
            Personaje *plaqueta = crea_plaqueta();
            NodoJerarquia *nodo_plaqueta = crea_nodo_jerarquia(1000 + i, 1, plaqueta);
            
            //Movimiento sinusoidal vertical
            double offset_y = sin(t * PI * 3 + i * PI / 2.5) * 60.0;
            nodo_plaqueta->pos_x = pos_plaquetas[i][0];
            nodo_plaqueta->pos_y = pos_plaquetas[i][1] + offset_y;
            nodo_plaqueta->escala = 100.0;
            nodo_plaqueta->rot_z = t * 180.0 + i * 72.0; //Rotacion lenta
            
            asigna_material_personaje(plaqueta, mat_plaqueta);
            agrega_hijo_jerarquia(nodo_vena, nodo_plaqueta);
        }
        
        //Crea y posicionar 25 globulos blancos
        for(int i = 0; i < 25; i++) 
        {
            Personaje *globulo_blanco = crea_globulo_blanco();
            NodoJerarquia *nodo_blanco = crea_nodo_jerarquia(2000 + i, 1, globulo_blanco);
            
            //Movimiento sinusoidal horizontal y vertical
            double offset_x = cos(t * PI * 2.5 + i * PI / 2) * 50.0;
            double offset_y = sin(t * PI * 2 + i * PI / 2) * 70.0;
            nodo_blanco->pos_x = pos_blancos[i][0] + offset_x;
            nodo_blanco->pos_y = pos_blancos[i][1] + offset_y;
            nodo_blanco->escala = 100.0;
            nodo_blanco->rot_z = t * 120.0 + i * 72.0;
            
            asigna_material_personaje(globulo_blanco, mat_globulo_blanco);
            agrega_hijo_jerarquia(nodo_vena, nodo_blanco);
        }
        
        //Mr. Atomix
        Personaje *mr_atomix = crea_mr_atomix();
        
        //Asigna materiales a las partes de Mr. Atomix
        Personaje *torso = busca_parte_personaje(mr_atomix, "torso");

        if(torso) 
            asigna_material_personaje(torso, mat_traje);
        
        Personaje *cuello = busca_parte_personaje(mr_atomix, "cuello");

        if(cuello) 
            asigna_material_personaje(cuello, mat_traje);

        Personaje *cabeza = busca_parte_personaje(mr_atomix, "cabeza");

        if(cabeza) 
            asigna_material_personaje(cabeza, mat_casco);
        
        Personaje *brazo_izq = busca_parte_personaje(mr_atomix, "brazo_izquierdo");

        if(brazo_izq) 
            asigna_material_personaje(brazo_izq, mat_traje);
        
        Personaje *brazo_der = busca_parte_personaje(mr_atomix, "brazo_derecho");

        if(brazo_der) 
            asigna_material_personaje(brazo_der, mat_traje);
        
        Personaje *codo_izq = busca_parte_personaje(mr_atomix, "codo_izquierdo");

        if(codo_izq) 
            asigna_material_personaje(codo_izq, mat_traje);
        
        Personaje *codo_der = busca_parte_personaje(mr_atomix, "codo_derecho");

        if(codo_der) 
            asigna_material_personaje(codo_der, mat_traje);
        
        Personaje *mano_izq = busca_parte_personaje(mr_atomix, "mano_izquierda");

        if(mano_izq) 
            asigna_material_personaje(mano_izq, mat_guantes);
        
        Personaje *mano_der = busca_parte_personaje(mr_atomix, "mano_derecha");

        if(mano_der) 
            asigna_material_personaje(mano_der, mat_guantes);
        
        Personaje *pierna_izq = busca_parte_personaje(mr_atomix, "pierna_izquierda");

        if(pierna_izq) 
            asigna_material_personaje(pierna_izq, mat_traje);
        
        Personaje *pierna_der = busca_parte_personaje(mr_atomix, "pierna_derecha");

        if(pierna_der) asigna_material_personaje(pierna_der, mat_traje);
        
        Personaje *rodilla_izq = busca_parte_personaje(mr_atomix, "rodilla_izquierda");

        if(rodilla_izq) 
            asigna_material_personaje(rodilla_izq, mat_traje);
        
        Personaje *rodilla_der = busca_parte_personaje(mr_atomix, "rodilla_derecha");

        if(rodilla_der) 
            asigna_material_personaje(rodilla_der, mat_traje);
        
        Personaje *pie_izq = busca_parte_personaje(mr_atomix, "pie_izquierdo");

        if(pie_izq) 
            asigna_material_personaje(pie_izq, mat_guantes);
        
        Personaje *pie_der = busca_parte_personaje(mr_atomix, "pie_derecho");

        if(pie_der) 
            asigna_material_personaje(pie_der, mat_guantes);
        
        //Dialogo
        if(f < 330) 
        {
            char *dialogos2[] = 
            {
                "Uf! Aqui estamos dentro",
                "del cuerpo humano. Esos",
                "salvavidas rojos son",
                "globulos rojos, llevan",
                "oxigeno a tu sangre."
            };
            
            Dialogo *dialogo_frame = crea_dialogo(5, dialogos2, audio_dialogo);

            if(dialogo_frame != NULL) 
                muestra_dialogo(mr_atomix, dialogo_frame);
            
            for(int i = 0; i < 4; i++) 
            {
                strcpy(dialogo_frame->lineas[i], dialogos2[i]);
                dialogo_frame->total_caracteres += strlen(dialogos2[i]);
            }
            
            dialogo_frame->audio = audio_dialogo;
            dialogo_frame->activo = true;
            dialogo_frame->tiempo_mostrado = f * duracion_frame;
            mr_atomix->dialogo = dialogo_frame;
        } 
        else 
            mr_atomix->dialogo = NULL;

        NodoJerarquia *nodo_atomix = crea_nodo_jerarquia(3500, 1, mr_atomix);
        
        //Movimiento horizontal y vertical (nadando)
        double movimiento_nado_y = sin(t * PI * 4) * 40.0; // Movimiento vertical ondulante
        nodo_atomix->pos_x = 200.0 + t * 800.0;
        nodo_atomix->pos_y = 350.0 + movimiento_nado_y;
        nodo_atomix->escala = 14.0; // Mitad de la escala original
        
        //Animacion de nado (brazos y piernas sincronizados)
        double ciclo_nado = sin(t * PI * 12) * 20.0; // Ciclo más rápido para nado
        
        // Anima brazos (brazadas de nado)
        Personaje *brazo_izq_anim = busca_parte_personaje(mr_atomix, "brazo_izquierdo");
        Personaje *brazo_der_anim = busca_parte_personaje(mr_atomix, "brazo_derecho");

        if(brazo_izq_anim) 
            brazo_izq_anim->angulo_actual = ciclo_nado + 30.0;

        if(brazo_der_anim) 
            brazo_der_anim->angulo_actual = -ciclo_nado - 30.0;
        
        //Anima piernas (patadas de nado)
        Personaje *pierna_izq_anim = busca_parte_personaje(mr_atomix, "pierna_izquierda");
        Personaje *pierna_der_anim = busca_parte_personaje(mr_atomix, "pierna_derecha");

        if(pierna_izq_anim) 
            pierna_izq_anim->angulo_actual = -ciclo_nado * 0.7;

        if(pierna_der_anim) 
            pierna_der_anim->angulo_actual = ciclo_nado * 0.7;
        
        //Anima codos
        Personaje *codo_izq_anim = busca_parte_personaje(mr_atomix, "codo_izquierdo");
        Personaje *codo_der_anim = busca_parte_personaje(mr_atomix, "codo_derecho");

        if(codo_izq_anim) 
            codo_izq_anim->angulo_actual = fabs(ciclo_nado) * 0.8;

        if(codo_der_anim) 
            codo_der_anim->angulo_actual = fabs(ciclo_nado) * 0.8;
        
        //Anima rodillas
        Personaje *rodilla_izq_anim = busca_parte_personaje(mr_atomix, "rodilla_izquierda");
        Personaje *rodilla_der_anim = busca_parte_personaje(mr_atomix, "rodilla_derecha");

        if(rodilla_izq_anim) 
            rodilla_izq_anim->angulo_actual = fabs(ciclo_nado) * 0.5;

        if(rodilla_der_anim) 
            rodilla_der_anim->angulo_actual = fabs(ciclo_nado) * 0.5;
        
        //Movimiento de cabeza
        Personaje *cabeza_anim = busca_parte_personaje(mr_atomix, "cabeza");

        if(cabeza_anim) 
            cabeza_anim->angulo_actual = sin(t * PI * 6) * 8.0;
        
        agrega_hijo_jerarquia(nodo_vena, nodo_atomix);
        
        //Crear y posicionar 25 globulos rojos
        for(int i = 0; i < 25; i++) 
        {
            Personaje *globulo_rojo = crea_globulo_rojo();
            NodoJerarquia *nodo_rojo = crea_nodo_jerarquia(3000 + i, 1, globulo_rojo);
            
            //Movimiento sinusoidal
            double offset_x = cos(t * PI * 3 + i * PI / 2.5) * 40.0;
            double offset_y = sin(t * PI * 2.5 + i * PI / 3) * 80.0;
            nodo_rojo->pos_x = pos_rojos[i][0] + offset_x;
            nodo_rojo->pos_y = pos_rojos[i][1] + offset_y;
            nodo_rojo->escala = 100.0;
            nodo_rojo->rot_z = t * 200.0 + i * 72.0;
            
            asigna_material_personaje(globulo_rojo, mat_globulo_rojo);
            agrega_hijo_jerarquia(nodo_vena, nodo_rojo);
        }
        
        Luz *luz_principal_frame = (Luz*)malloc(sizeof(Luz));
        *luz_principal_frame = *luz_principal;
        
        Luz *luz_relleno_frame = (Luz*)malloc(sizeof(Luz));
        *luz_relleno_frame = *luz_relleno;
        
        NodoJerarquia *nodo_luz1 = crea_nodo_jerarquia(5000, 3, luz_principal_frame);
        NodoJerarquia *nodo_luz2 = crea_nodo_jerarquia(5001, 3, luz_relleno_frame);
        
        nodo_luz1->pos_x = 500.0 + cos(t * PI * 2) * 150.0;
        nodo_luz1->pos_y = 800.0;
        nodo_luz1->pos_z = 400.0;
        
        nodo_luz2->pos_x = -300.0;
        nodo_luz2->pos_y = 600.0;
        nodo_luz2->pos_z = 300.0;
        
        agrega_hijo_jerarquia(nodo_vena, nodo_luz1);
        agrega_hijo_jerarquia(nodo_vena, nodo_luz2);
        
        Frame *frame = crea_frame(f + 1, nodo_vena, duracion_frame);
        agrega_frame_escena(escena_animacion, frame);
    }
    
    free(mat_vena);
    free(mat_globulo_rojo);
    free(mat_globulo_blanco);
    free(mat_plaqueta);
    free(mat_traje);
    free(mat_casco);
    free(mat_guantes);
    free(luz_principal);
    free(luz_relleno);
    
    encola_escena(pelicula_global, escena_animacion);
}

Personaje *crea_celula() 
{
    //Celula es un rectangulo grande que cubre toda las escena
    Punto *rot_celula = crea_punto(900, 0.0, 0.0, 0, 0, 0);
    Personaje *celula = crea_personaje(3000, "fondo", rot_celula);
    free(rot_celula);

    Punto *pts_celula[] = 
    {
        crea_punto(901, -1000.0, -500.0, 0, 0.0, 0.0),
        crea_punto(902, 3000.0, -500.0, 0, 4.0, 0.0),
        crea_punto(903, 3000.0, 1500.0, 0, 4.0, 2.0),
        crea_punto(904, -1000.0, 1500.0, 0, 0.0, 2.0)
    };
    
    celula->num_puntos = 4;
    celula->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));
    
    for(int i = 0; i < 4; i++) 
    {
        celula->puntos_figura[i] = *pts_celula[i];
        free(pts_celula[i]);
    }
    
    //Convierte coordenadas a relativas
    convierte_absolutas_a_relativas_personaje(celula, 0.0, 0.0);
    
    //Asigna textura al fondo
    if(cola_recursos_global != NULL) 
    {
        celula->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/celula.jpg");
    }
    
    return celula;
}

Personaje *crea_adn()
{
    Textura *tex_adn = NULL;

    if (cola_recursos_global != NULL) 
        tex_adn = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/adn.jpg");

    Punto *rot_tira_central = crea_punto(5, 7.868733751428628, 7.020998037361379, 0, 0, 0);
    Personaje *tira_central = crea_personaje(10000, "tira_central", rot_tira_central);
    free(rot_tira_central);

    Punto *pts_tira_central[] = 
    {
        crea_punto(1, 7.199145855194124, 8.027532004197273, 0, 0.0, 1.0),
        crea_punto(2, 6.808109589041096, 7.677041095890411, 0, 0.0, 0.0),
        crea_punto(3, 8.39064399770247, 6.22159586444572, 0, 1.0, 0.0),
        crea_punto(4, 8.84727863751285, 6.448090462034285, 0, 1.0, 1.0)
    };

    tira_central->num_puntos = 4;

    tira_central->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        tira_central->puntos_figura[i] = *pts_tira_central[i];
        free(pts_tira_central[i]);
    }
    
    tira_central->textura = tex_adn;
    tira_central->escala_x = 1.0;
    tira_central->escala_y = 1.0;

    Punto *rot_tira_izq = crea_punto(10, 8.871011092575511, 7.502309400946359, 0, 0, 0);
    Personaje *tira_arriba_izq = crea_personaje(10001, "tira_arriba_izq", rot_tira_izq);
    free(rot_tira_izq);

    Punto *pts_tira_izq[] = 
    {
        crea_punto(6, 8.26, 9.88, 0, 0.0, 1.0),
        crea_punto(7, 8.74, 9.76, 0, 0.0, 0.0),
        crea_punto(8, 9.32, 5.56, 0, 1.0, 0.0),
        crea_punto(9, 8.90, 6.14, 0, 1.0, 1.0)
    };

    tira_arriba_izq->num_puntos = 4;

    tira_arriba_izq->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));

    for(int i = 0; i < 4; i++) 
    {
        tira_arriba_izq->puntos_figura[i] = *pts_tira_izq[i];
        free(pts_tira_izq[i]);
    }
    
    tira_arriba_izq->textura = tex_adn;
    agrega_hijo_personaje(tira_central, tira_arriba_izq);

    
    Punto *rot_tira_der = crea_punto(15, 7.844458686577263, 8.313285801684973, 0, 0, 0);
    Personaje *tira_arriba_der = crea_personaje(10002, "tira_arriba_der", rot_tira_der);
    free(rot_tira_der);

    Punto *pts_tira_der[] = 
    {
        crea_punto(11, 10.24, 8.68, 0, 0.0, 1.0),
        crea_punto(12, 10.66, 8.42, 0, 0.0, 0.0),
        crea_punto(13, 6.78, 7.98, 0, 1.0, 0.0),
        crea_punto(14, 6.32, 8.32, 0, 1.0, 1.0) 
    };

    tira_arriba_der->num_puntos = 4;
    tira_arriba_der->puntos_figura = (Punto*)malloc(4 * sizeof(Punto));
    for(int i = 0; i < 4; i++) {
        tira_arriba_der->puntos_figura[i] = *pts_tira_der[i];
        free(pts_tira_der[i]);
    }

    tira_arriba_der->textura = tex_adn;
    agrega_hijo_personaje(tira_central, tira_arriba_der);

    convierte_absolutas_a_relativas_personaje(tira_central, 0.0, 0.0);

    return tira_central;
}


Personaje *crea_mitocondria() 
{
    Punto *rot_mitocondria = crea_punto(100, 6.1999002993464485, 5.005587609794676, 0.0, 0.0, 0.0);
    
    Personaje *mitocondria = crea_personaje(17, "mitocondria", rot_mitocondria);
    free(rot_mitocondria);

    Punto *pts_mitocondria[] = 
    {
        crea_punto(1, 6.00, 8.00, 0.0, 0.439, 0.902),
        crea_punto(2, 7.16, 8.68, 0.0, 0.624, 1.000),
        crea_punto(3, 8.54, 8.62, 0.0, 0.844, 0.988),
        crea_punto(4, 9.14, 7.88, 0.0, 0.939, 0.884),
        crea_punto(5, 9.52, 7.38, 0.0, 1.000, 0.812),
        crea_punto(6, 8.74, 5.40, 0.0, 0.875, 0.528),
        crea_punto(7, 7.00, 3.00, 0.0, 0.598, 0.181),
        crea_punto(8, 6.00, 2.00, 0.0, 0.439, 0.037),
        crea_punto(9, 4.62, 1.74, 0.0, 0.220, 0.000),
        crea_punto(10, 3.70, 2.14, 0.0, 0.073, 0.058),
        crea_punto(11, 3.24, 2.98, 0.0, 0.000, 0.178),
        crea_punto(12, 3.48, 3.88, 0.0, 0.038, 0.308)
    };
    
    int num_puntos = 12;

    mitocondria->num_puntos = num_puntos;

    mitocondria->puntos_figura = (Punto*)malloc(num_puntos * sizeof(Punto));
    
    if(mitocondria->puntos_figura == NULL) 
    {
        free_personaje(mitocondria); 

        for(int i = 0; i < num_puntos; i++) 
            free_punto(pts_mitocondria[i]);

        return NULL;
    }

    for(int i = 0; i < num_puntos; i++) 
        mitocondria->puntos_figura[i] = *pts_mitocondria[i];

    for(int i = 0; i < num_puntos; i++)
        free_punto(pts_mitocondria[i]);

    convierte_absolutas_a_relativas_personaje(mitocondria, 0.0, 0.0);

    if(cola_recursos_global != NULL) 
    {
        mitocondria->textura = busca_textura_en_cola(cola_recursos_global, "Figuras/Texturas/mitocondria.jpg");
    }

    return mitocondria;
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

    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    glViewport(0, 0, 800, 600);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    if(!inicializa_audio()) 
    {
        puts("Error al inicializar audio");
        return 1;
    }
    
    ajusta_volumen_global(10);

    cola_recursos_global = crea_cola_recursos();

    encola_todas_las_texturas(cola_recursos_global);

    cargar_recursos(cola_recursos_global);
    
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

    pelicula_global = crea_pelicula();

    visualiza_escena1();

    visualiza_escena2();

    escena_actual = pelicula_global->frente;
    
    if(escena_actual != NULL)
        frame_actual = escena_actual->primer_frame;

    pila_deshacer = crea_pila_frames();
    ultimo_tiempo = glutGet(GLUT_ELAPSED_TIME);
    en_pausa = false;
    
    inicializa_botones();

    inicializa_camara();
    
    //Timer global
    glutTimerFunc(33, actualiza, 0);
    
    glutMainLoop();
    
    cierra_audio();
    free_cola_recursos(cola_recursos_global);
    free_pelicula(pelicula_global);
    free_pila_frames(pila_deshacer);
    
    return 0;
}
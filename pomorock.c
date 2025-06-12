// pomorock.c

/*
 * Pomorock - Pomodoro-style focus timer
 * A useful program to learn about Unix processes, signals, multiplexed I/O
 * and interprocess communications
 * Copyright 2025 Manuel Prieto-Matias
 * Inspired by tomatoshell (https://github.com/LytixDev/tomatoshell)
 * by Nicolai Brand (2022), licensed under GPL.
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXARGPLAYER 5 // argumentos que recibe el audio player

#define ALARMSONG "./pomorock.mp3" // Este es el mp3 file que suena al vencer el pomodoro
#define AMBIENTSONG "./ambient.mp3" // Si se activa musica de ambiente esto esta es la musica que sonora de fondo
#define LOGFILE "./pomolog.csv" // Fichero de log donde ir guardando el progreso

// Configuración por defecto
int session_time = 50 * 60; // Tiempo de la sesion en segundos, por defecto 50 minutos
int break_time = 5 * 60;    // Tiempo del descanso en segundos, por defecto 5 minutos
int total_sessions = 3;     // Numero de sesiones antes e finalizar

char *alarm_file = NULL;
char *ambient_file = NULL;

char *log_file = NULL;
char *audio_player[MAXARGPLAYER] = {NULL}; // MAXARGPLAYER-1 argumentos + NULL

// Terminal original
struct termios orig_termios;

// Prototipos de funciones
void usage(const char *progname);
void parse_args(int argc, char *argv[]);
void find_audio_player();
void run_timer(int duration, const char *message, char *song);
void play_alarm();
void play_ambient();
void write_log();
void total_hours_used();
void cleanup();
void handle_sigint(int sig);

int main(int argc, char *argv[]) {

    signal(SIGINT, handle_sigint);  // manejamos la señal SIGINT
    tcgetattr(STDIN_FILENO, &orig_termios); // Guardar configuración terminal


    parse_args(argc, argv);
    find_audio_player();

    // Desactivar eco y cursor
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); //modo sin eco y sin buffer de línea
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\e[?25l"); // ocultar cursor

    for (int i = 1; i <= total_sessions; ++i) {
        printf("🍅 Session %d/%d starting...\n", i, total_sessions);
        run_timer(session_time, "Session",ambient_file);

        if (i < total_sessions) {
            printf("🌿 Break starting...\n");
            run_timer(break_time, "Break",alarm_file);
        }
    }

    write_log();
    cleanup();
    return 0;
}

void usage(const char *progname) {
    printf("Usage: %s [-t minutes] [-d break_minutes] [-n sessions] [-r] [-h]\n", progname);
    exit(0);
}

void parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "t:d:n:frh")) != -1) {
        switch (opt) {
            case 't': session_time = atoi(optarg) * 60; break;
            case 'd': break_time = atoi(optarg) * 60; break;
            case 'n': total_sessions = atoi(optarg); break;
            case 'r': total_hours_used(); exit(0); break;
            case 'h': usage(argv[0]); break;
            default: usage(argv[0]);
        }
    }
    // TODO, config via options
    alarm_file = strdup(ALARMSONG);
    log_file = strdup(LOGFILE);
    ambient_file =strdup(AMBIENTSONG);
}

void find_audio_player() {
    // Probar algunos reproductores comunes
    if (!access("/usr/bin/mpv", X_OK)) {
        audio_player[0] = "mpv";
        audio_player[1] = "--really-quiet";
        audio_player[2] = "--loop=inf";
        audio_player[3] = NULL; //song
        audio_player[4] = NULL;

    // TODO, review options for loop playing in paplay, pw-play and aplay
    // else if (!access("/usr/bin/paplay", X_OK)) audio_player = "paplay";
    // else if (!access("/usr/bin/pw-play", X_OK)) audio_player = "pw-play";
    // else if (!access("/usr/bin/aplay", X_OK)) audio_player = "aplay";
    } else {
        audio_player[0] = NULL;
    }
}

void run_timer(int duration, const char *message, char *song) {
    time_t start = time(NULL);
    pid_t music_pid = -1;

    // Lanzar música de fondo si hay reproductor y mp3 disponible
    if (audio_player[0] && song) {
        music_pid = fork();
        if (music_pid == 0) {
            // Proceso hijo: reproducir música en segundo plano
            audio_player[3]=song;
            execvp(audio_player[0], audio_player);
            exit(EXIT_FAILURE); // Si falla execlp
        }
    }

    // Temporizador con cuenta atrás simple
    while (time(NULL) - start < duration) {
        int remaining = duration - (int)(time(NULL) - start);
        int minutes = remaining / 60;
        int seconds = remaining % 60;

        printf("\r⏳ %02d:%02d remaining... ", minutes, seconds);
        fflush(stdout);

        // Ver si el usuario pulsa 's' o 'q'
        fd_set read_fds;
        struct timeval tv = {1, 0}; // Esperar máximo 1s

        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        if (select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv) > 0) {
            char c;
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n > 0) {
                if (c == 's') break;
                if (c == 'q') {
                    if (music_pid > 0) kill(music_pid, SIGKILL); //mandamos SIGKILL al hijo
                    waitpid(music_pid, NULL, 0);
                    cleanup();
                    exit(0);
                }
            }
        }
    }
    printf("\n%s finished!\n", message);
    // Terminar música de fondo si aún está sonando
    if (music_pid > 0) {
        kill(music_pid, SIGKILL);
        waitpid(music_pid, NULL, 0);
    }
}


void write_log() {
    FILE *f = fopen(log_file, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(f, "%04d-%02d-%02d,%d,%d\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            total_sessions, session_time / 60);
    fclose(f);
}

void total_hours_used() {
    FILE *f = fopen(log_file, "r");
    if (!f) return;
    int sessions, time;
    long total_seconds = 0;
    while (fscanf(f, "%*[^,],%d,%d", &sessions, &time) == 2) {
        total_seconds += sessions * time * 60;
    }
    fclose(f);
    printf("Total hours spent focused: %ldh 🍅🤓\n", total_seconds / 3600);
}

void cleanup() {

    // Restaurar terminal
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    // Mostrar el cursor (en caso de que lo ocultaras)
    printf("\e[?25h");

    fflush(stdout);
}

void handle_sigint(int sig) {
    cleanup();
    printf("\n\033[0;31m⛔ Interrumpido con Ctrl+C\033[0m\n");
    exit(0);
}

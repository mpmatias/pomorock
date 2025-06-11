// pomorock.c

/*
 * Pomorock - Pomodoro-style focus timer
 *
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


#define ALARMSONG "./pomorock.mp3"

// Configuración por defecto
int session_time = 50 * 60; // Tiempo de la sesion en segundos, por defecto 50 minutos
int break_time = 5 * 60;    // Tiempo del descanso en segundos, por defecto 5 minutos
int total_sessions = 3;
char *alarm_file = NULL;
char *log_file = NULL;
char *audio_player = NULL;

// Terminal original
struct termios orig_termios;

// Prototipos de funciones
void usage(const char *progname);
void parse_args(int argc, char *argv[]);
void find_audio_player();
void run_timer(int duration, const char *message);
void play_alarm();
void write_log();
void total_hours_used();
void cleanup();
void handle_sigint(int sig);

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    tcgetattr(STDIN_FILENO, &orig_termios); // Guardar configuración terminal
    parse_args(argc, argv);
    find_audio_player();

    // Desactivar eco y cursor
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\e[?25l"); // ocultar cursor

    for (int i = 1; i <= total_sessions; ++i) {
        printf("🍅 Session %d/%d starting...\n", i, total_sessions);
        run_timer(session_time, "Session");

        if (i < total_sessions) {
            printf("🌿 Break starting...\n");
            run_timer(break_time, "Break");
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

    alarm_file = strdup("./pomorock.mp3");
    log_file = strdup("./pomolog.csv");
}

void find_audio_player() {
    // Probar algunos reproductores comunes
    if (!access("/usr/bin/mpv", X_OK)) audio_player = "mpv";
    else if (!access("/usr/bin/paplay", X_OK)) audio_player = "paplay";
    else if (!access("/usr/bin/pw-play", X_OK)) audio_player = "pw-play";
    else if (!access("/usr/bin/aplay", X_OK)) audio_player = "aplay";
    else audio_player = NULL;
}

void run_timer(int duration, const char *message) {
    // TODO: Mostrar temporizador con cuenta regresiva
    // TODO: Leer tecla 's' para saltar o 'q' para salir anticipadamente
    // TODO: Notificaciones opcionales
    // TODO: Musica de fondo

    time_t start = time(NULL);
    while (time(NULL) - start < duration) {
        // TODO: Imprimir tiempo restante
        sleep(1);
    }

    printf("%s finished!\n", message);
    play_alarm();
}

void play_alarm() {
    if (!audio_player || !alarm_file) return;

    pid_t pid = fork();
    if (pid == 0) {
        execlp(audio_player, audio_player, alarm_file, (char *)NULL);
        _exit(EXIT_FAILURE); // por si falla execlp
    } else if (pid > 0) {
        printf("Press 's' to stop alarm.\n");
        while (1) {
            char c = getchar();
            if (c == 's') {
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                break;
            }
        }
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
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // Restaurar terminal
    printf("\e[?25h"); // mostrar cursor
}

void handle_sigint(int sig) {
    cleanup();
    printf("\nExiting.\n");
    exit(0);
}

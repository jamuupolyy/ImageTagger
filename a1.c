/*

COMP30023: Computer Systems, 2019 Semester 1

image_tagger HTTP game implementation.

@author: Jason Polychronopoulos
@studentID: 921565

Code referenced:
- http-server.c from Lab 6

*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_200_FORMAT_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Set-Cookie: user_id=%d\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

// Define size constraints
#define BUFFER_SIZE 2049
#define MAX_COOKIES 10
#define MAX_KEYWORDS 20
#define MAX_WORD_LEN 20
#define MAX_PLAYERS 2
#define MAX_WORDLIST_STRING 1024
#define HTML_WORDLIST_SHIFT 265
#define HTML_USERNAME_SHIFT 213

// Define initialiasation flag for keyword counts
#define INIT_FLAG -1

// Define strstr constants that will determine page redirects and parsing
#define START "?start=Start"
#define QUIT "quit="
#define KEYWORD "keyword="
#define USERNAME "user="
#define USER_COOKIE "user_id="
#define GUESS "guess="
#define END_KEYWORD '&'
#define EMPTY_STRING " "
#define CRLF "\r\n\r\n"

// Define formatting constants for HTML pages
#define HTML_H3 "<h3>"
#define HTML_H3_END "</h3>"

// represents the types of method
typedef enum {
    GET,
    POST,
    UNKNOWN
} METHOD;

int parse_header(char * curr, char * check) {

  // Takes the current buffer and a string to check what html page to send
  if (!strncmp(curr, check, strlen(check))) {
    return true;
  }
  return false;
}

char * parse_http_body(char * buffer) {

  // Function to parse the body of a http response message.
  char * http_body = strstr(buffer, CRLF) + strlen(CRLF);

  return http_body;
}

char * parse_keyword(char * http_body) {

  // function to parse the submitted keyword from the http response message.
  char * keyword = NULL;
  char * start_keyword = strstr(http_body, KEYWORD) + strlen(KEYWORD);

  int keyword_len = 0;
  while (start_keyword[keyword_len] != END_KEYWORD) {
    keyword_len++;
  }

  keyword = (char *) malloc (keyword_len);

  int j;
  for (j = 0; j < keyword_len; j++) {
    keyword[j] = start_keyword[j];
  }

  keyword[j] = '\0';

  return keyword;
}

int parse_cookie(char * buff) {

  // function to get the cookie of a user from a http header.
  char * get_cookie = strstr(buff, USER_COOKIE) + strlen(USER_COOKIE);

  // convert to an integer and return.
  int player_cookie = atoi(get_cookie);

  return player_cookie;
}

char * get_round_html(int page_number, int * game_round) {

  // Function to get the next rounds image.
  char * html_file;

  if (page_number == 3) {
    if (*game_round == 1) {
      html_file = "3_first_turn_1.html";
    }

    if (*game_round == 2) {
      html_file = "3_first_turn_2.html";
    }

    if (*game_round == 3) {
      html_file = "3_first_turn_3.html";
    }

    if (*game_round == 4) {
      html_file = "3_first_turn_4.html";
    }
  }

  else if (page_number == 4) {
    if (*game_round == 1) {
      html_file = "4_accepted_1.html";
    }
    if (*game_round == 2) {
      html_file = "4_accepted_2.html";
    }
    if (*game_round == 3) {
      html_file = "4_accepted_3.html";
    }
    if (*game_round == 4) {
      html_file = "4_accepted_4.html";
    }
  }

  else if (page_number == 5) {
    if (*game_round == 1) {
      html_file = "5_discarded_1.html";
    }
    if (*game_round == 2) {
      html_file = "5_discarded_2.html";
    }
    if (*game_round == 3) {
      html_file = "5_discarded_3.html";
    }
    if (*game_round == 3) {
      html_file = "5_discarded_4.html";
    }
  }
  return html_file;
}

int send_header(int sockfd, char * buff, char * html_file, int n) {

  // get the size of the file
  struct stat st;
  stat(html_file, &st);

  n = sprintf(buff, HTTP_200_FORMAT, st.st_size);

  // send the header first
  if (write(sockfd, buff, n) < 0) {
      perror("write");
      return false;
  }
  return true;
}

int send_header_cookie(int sockfd, char * buff, char * html_file, int n, int * c) {

  // get the size of the file
  struct stat st;
  stat(html_file, &st);

  n = sprintf(buff, HTTP_200_FORMAT_COOKIE, *c, st.st_size);

  // send the header first
  if (write(sockfd, buff, n) < 0) {
      perror("write");
      return false;
  }
  return true;
}

int send_file(int sockfd, char * html_file, int n) {

  // send the file
  int filefd = open(html_file, O_RDONLY);
  do {
      n = sendfile(sockfd, filefd, NULL, BUFFER_SIZE - 1);
  }

  while (n > 0);
  if (n < 0) {
      perror("sendfile");
      close(filefd);
      return false;
  }
  close(filefd);
  return true;
}

int send_html_username(int sockfd, char * buff, char * html_file, int n, char * username) {

    // Function to send the header and html file with the username added

    // calculate the added buffer length
    int username_length = strlen(username);
    int html_h_size = strlen(HTML_H3) + strlen(HTML_H3_END);
    int added_length = username_length + html_h_size;

    // format the username to have H3 html formatting
    char username_formatted[added_length];
    strcpy(username_formatted, HTML_H3);
    strcat(username_formatted, username);
    strcat(username_formatted, HTML_H3_END);

    // get the size of the file
    struct stat st;
    stat(html_file, &st);

    // increase file size to accommodate the username
    long size = st.st_size + added_length;
    n = sprintf(buff, HTTP_200_FORMAT, size);

    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }

    // read the content of the HTML file
    int filefd = open(html_file, O_RDONLY);
    n = read(filefd, buff, BUFFER_SIZE - 1);
    if (n < 0)
    {
        perror("read");
        close(filefd);
        return false;
    }
    close(filefd);

    // move the trailing part backward
    int p1, p2;
    for (p1 = size - 1, p2 = p1 - added_length; p1 >= size - HTML_USERNAME_SHIFT; --p1, --p2)
        buff[p1] = buff[p2];
    ++p2;
    buff[p2++] = ' ';

    // copy the username
    strncpy(buff + p2, username_formatted, added_length);
    if (write(sockfd, buff, size) < 0)
    {
        perror("write");
        return false;
    }
    return true;
}

int send_html_wordlist(int sockfd,
  char * buff,
  char * html_file,
  int n, int player,
  char keyword_list[MAX_PLAYERS][MAX_KEYWORDS][MAX_WORD_LEN],
  int count_assignment[MAX_PLAYERS]) {

      // function to send header and html file to client with
      // all submitted words printed out

      // create a string that will copy the entire list of currently submitted
      // words
      char wordlist_string[MAX_WORDLIST_STRING];
      int i;
      for (i = 0; i < count_assignment[player]; i++) {

        // if it is the first word in the list, don't add ", "
        if (!i) {
          strcpy(wordlist_string, keyword_list[player][i]);
        }

        // concatenate the rest of the words
        else {
          strcat(wordlist_string, ", ");
          strcat(wordlist_string, keyword_list[player][i]);
        }

      }

      // calculate the added length to the buffer
      int html_h_size = strlen(HTML_H3) + strlen(HTML_H3_END);
      int wordlist_string_length = strlen(wordlist_string);
      int added_length = wordlist_string_length + html_h_size;

      // format the word list with H3 html formatting
      char word_list_formatted[added_length];
      strcpy(word_list_formatted, HTML_H3);
      strcat(word_list_formatted, wordlist_string);
      strcat(word_list_formatted, HTML_H3_END);

      // get the size of the file
      struct stat st;
      stat(html_file, &st);

      // increase file size to accommodate the username
      long size = st.st_size + added_length;
      n = sprintf(buff, HTTP_200_FORMAT, size);

      // send the header first
      if (write(sockfd, buff, n) < 0)
      {
          perror("write");
          return false;
      }

      // read the content of the HTML file
      int filefd = open(html_file, O_RDONLY);
      n = read(filefd, buff, BUFFER_SIZE - 1);
      if (n < 0)
      {
          perror("read");
          close(filefd);
          return false;
      }
      close(filefd);

      // move the trailing part backward
      int p1, p2;
      for (p1 = size - 1, p2 = p1 - added_length;
        p1 >= size - HTML_WORDLIST_SHIFT; --p1, --p2) {
          buff[p1] = buff[p2];
      }
      ++p2;
      buff[p2++] = ' ';

      // copy the wordlist
      strncpy(buff + p2, word_list_formatted, added_length);
      if (write(sockfd, buff, size) < 0) {
          perror("write");
          return false;
      }

      return true;
}

bool check_wordlist(
  char keyword_list[MAX_PLAYERS][MAX_KEYWORDS][MAX_WORD_LEN],
  int player,
  int count_assignment[MAX_PLAYERS]) {

    // initialise the wordlist index of the other player
    int other_player;
    if (!player) {
      other_player = 1;
    }

    else if (player) {
      other_player = 0;
    }

    // loop through both players wordlists, if a matching word is found, return true
    int i, j;
    for (i = 0; i < count_assignment[player]; i++) {
      for (j = 0; j < count_assignment[other_player]; j++) {
        if (!strcmp(keyword_list[player][i], keyword_list[other_player][j])) {
          return true;
        }
      }
    }
    return false;
}

void reset_lists (
  char keyword_list[MAX_PLAYERS][MAX_KEYWORDS][MAX_WORD_LEN],
  int keyword_count[MAX_COOKIES],
  int count_assignment[MAX_PLAYERS]) {

    // function on game over to clear all lists for the next round
    int i, j;
    for (i = 0; i < MAX_PLAYERS; i++) {
      count_assignment[i] = 0;
      for (j = 0; j < MAX_KEYWORDS; j++) {
        memset(keyword_list[i][j], 0, MAX_WORD_LEN);
      }
    }

    for (i = 0; i < MAX_COOKIES; i++) {
      keyword_count[i] = INIT_FLAG;
    }
}

static bool handle_http_request (
  int sockfd,
  int * cookie,
  int * win,
  int * game_round,
  char players[MAX_COOKIES][MAX_WORD_LEN],
  char keyword_list[MAX_PLAYERS][MAX_KEYWORDS][MAX_WORD_LEN],
  int keyword_count[MAX_COOKIES],
  int player_session[MAX_COOKIES],
  int count_assignment[MAX_PLAYERS],
  int win_check[MAX_PLAYERS],
  int * current_players,
  int * game_over) {

    // Main control function which handles all http request scenarios
    // and game flow.

    // try to read the request
    char buff[BUFFER_SIZE];
    int n = read(sockfd, buff, BUFFER_SIZE);
    if (n <= 0) {
        if (n < 0)
            perror("read");
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;

    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0) {
        curr += 4;
        method = GET;
    }

    else if (strncmp(curr, "POST ", 5) == 0) {
        curr += 5;
        method = POST;
    }

    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0) {
        perror("write");
        return false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/') {
        ++curr;
    }

    // game over
    if (*game_over) {
      char * html_file;
      html_file = "7_gameover.html";
      send_header(sockfd, buff, html_file, n);
      send_file(sockfd, html_file, n);

      *game_over = 1;
      return false;
    }

    // condition to let all players know the game has been completed
    if (*win) {

      // game is considered won until both players are on the won screen
      bool check = false;
      int num_won_players = 0;

      // get coooookie
      int player_cookie = parse_cookie(curr);
      int current_player = player_session[player_cookie];

      // current player has seen win screen, game can restart if both players
      // have seen it
      win_check[current_player] = 1;

      // if both players have seen the win screen, then game can restart
      int i;
      for (i = 0; i < MAX_PLAYERS; i++) {
        if (win_check[i]) num_won_players++;
      }

      if (num_won_players == MAX_PLAYERS) check=true;

      for (i = 0; i < MAX_COOKIES; i++) {
      }

      // game can restart if check == True, player binary assignments are reset
      if (check) {
        *win = 0;
        *current_players = 0;

        int j;
        for (j = 0; j < MAX_PLAYERS; j++) {
          win_check[j] = 0;
        }

        for (i = 0; i < MAX_COOKIES; i++) {
          player_session[i] = INIT_FLAG;
        }
      }

      char * html_file;
      html_file = "6_endgame.html";
      send_header(sockfd, buff, html_file, n);
      send_file(sockfd, html_file, n);
      return true;
    }

    if (parse_header(curr, " ")) {

        if (method == GET) {

          // if a player has a cookie set, skip the intro screen.
          if (strstr(curr, "Cookie")) {

            // get their username via cookie
            int player_cookie = parse_cookie(curr);

            char username[MAX_WORD_LEN];
            strcpy(username, players[player_cookie]);

            // get the size of the file
            char * html_file;
            html_file = "2_start.html";

            send_html_username(sockfd, buff, html_file, n, username);
          }

          else {

            // no cookie, welcome fresh blood
            parse_http_body(curr);
            char * html_file;
            html_file = "1_intro.html";

            // set their cookie, will only have a max of 10 cookies
            * cookie += 1;
            send_header_cookie(sockfd, buff, html_file, n, cookie);
            send_file(sockfd, html_file, n);
          }

        }

        else if (method == POST) {

          // mans want out
          if (strstr(curr, QUIT)) {

            char * html_file;
            html_file = "7_gameover.html";
            send_header(sockfd, buff, html_file, n);
            send_file(sockfd, html_file, n);

            *game_over = 1;
            return false;
          }

          else {

            // user submits their username
            char * username;
            username = (char *)malloc(sizeof(strstr(buff, USERNAME) + 5));
            strcpy(username, strstr(buff, USERNAME) + 5);

            // username is assigned to their cookie
            int player_cookie = parse_cookie(curr);
            strcpy(players[player_cookie], username);

            // get the size of the file
            char * html_file;
            html_file = "2_start.html";

            send_html_username(sockfd, buff, html_file, n, username);

          }
        }

        else {

          // for completeness
          fprintf(stderr, "no other methods supported");

        }
    }

    // player has pressed Start
    else if (parse_header(curr, START)) {

      if (method == GET) {

        parse_http_body(curr);

        // only two players may join
        if (*current_players <= MAX_PLAYERS) {

          int player_cookie = parse_cookie(curr);
          *current_players += 1;

          // if only 1 player, they will be assigned index 0 for the wordlist
          if (*current_players == 1) {
            player_session[player_cookie] = 0;
          }

          // the second player has joined, they get index 1 for the wordlist
          else if (*current_players == 2) {
            player_session[player_cookie] = 1;
          }

          int page_number = 3;
          char * html_file;
          html_file = get_round_html(page_number, game_round);
          send_header(sockfd, buff, html_file, n);
          send_file(sockfd, html_file, n);
        }

      }

      else if (method == POST) {

        // parse the http_body for submitted words
        char * http_body = (char *)malloc(sizeof(parse_http_body(curr)));
        strcpy(http_body, parse_http_body(curr));

        // player has left, game is over for everyone
        if (strstr(curr, QUIT)) {

          char * html_file;
          html_file = "7_gameover.html";
          send_header(sockfd, buff, html_file, n);
          send_file(sockfd, html_file, n);

          // it's over anakin, I have the high ground
          *game_over = 1;

          return false;

        }

        else if (strstr(curr, GUESS)) {

          // two players are in game, game begins
          if (*current_players == MAX_PLAYERS) {

            int player_cookie = parse_cookie(curr);

            // parse the keyword from the http body
            char * keyword;
            keyword = (char *) malloc (sizeof(parse_keyword(http_body)));
            strcpy(keyword, parse_keyword(http_body));

            // get the current player index for the wordlist
            int current_player = player_session[player_cookie];

            // if the keyword length is >= 1, then it is added to the wordlist
            if (strlen(keyword) >= 1) {

              // if a new unseen player submits a keyword, their count is set to 0
              if (keyword_count[player_cookie] == INIT_FLAG) {
                keyword_count[player_cookie] = 0;
              }

              // word count for the current player is used to submit their
              // keyword to the word list.
              int current_word_idx = keyword_count[player_cookie];
              strcpy(keyword_list[current_player][current_word_idx], keyword);
              keyword_count[player_cookie] += 1;
              count_assignment[current_player] = keyword_count[player_cookie];
            }

            // if a matching word is found, the game is won.
            if (check_wordlist(keyword_list, current_player, count_assignment)) {
              *win = 1;
            }

            // this returns endgame for the last person who submits a matching word
            if (*win) {
              char * html_file;
              html_file = "6_endgame.html";
              send_header(sockfd, buff, html_file, n);
              send_file(sockfd, html_file, n);

              // reset all the lists used to track keywords for the session
              reset_lists(keyword_list, keyword_count, count_assignment);

              // number of players is lowered
              *current_players -= 1;

              // if round is == 4,
              // then the image to be used will revert back to image-1
              if (*game_round != 4) {
                *game_round += 1;
              }

              else {
                *game_round = 1;
              }

              // current player has seen the win screen, they will be checked off
              win_check[current_player] = 1;

              return true;
            }

            else {

              // accepting a keyword and printing off the wordlist to the html page
              int page_number = 4;
              char * html_file;
              html_file = get_round_html(page_number, game_round);

              send_html_wordlist(sockfd, buff, html_file, n, current_player,
                keyword_list, count_assignment);
            }
          }

          // one player has not joined, word discarded
          else {
            int page_number = 5;
            char * html_file;
            html_file = get_round_html(page_number, game_round);
            send_header(sockfd, buff, html_file, n);
            send_file(sockfd, html_file, n);
          }

        }

        else {
          return false;
        }

      }
    }

    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0) {

        perror("write");
        return false;

    }

    return true;
}

int main(int argc, char * argv[]) {

    if (argc < 3) {
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0;
    }

    // create TCP socket which only accept IPv4
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // reuse the socket if possible
    int const reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("image_tagger server is now running at IP: %s on port %d\n",
                  argv[1], atoi(argv[2]));

    // listen on the socket
    listen(sockfd, 5);

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);

    // record the maximum socket number
    int maxfd = sockfd;

    // Session players
    int * current_players = (int *)malloc(sizeof(int));
    * current_players = 0;

    // Switch for game over
    int * game_over = (int *)malloc(sizeof(int));
    * game_over = 0;

    // variable to keep track of cookies
    int * cookie = (int *)malloc(sizeof(int));
    *cookie = 0;

    // variable to keep track of all session players' usernames
    char players[MAX_COOKIES][MAX_WORD_LEN];

    // variable to hold the wordlists
    char keyword_list[MAX_PLAYERS][MAX_KEYWORDS][MAX_WORD_LEN];

    // variable to hold # of keywords each player has submitted
    int keyword_count[MAX_COOKIES];
    int i;
    for (i = 0; i < MAX_COOKIES; i++) {
      keyword_count[i] = INIT_FLAG;
    }

    // maps counts for each cookie to the binary player for wordlist usage
    int count_assignment[MAX_PLAYERS];
    int count;
    for (count=0; count < MAX_PLAYERS; count++) {
      count_assignment[count] = 0;
    }

    // holds the player assignments for each wordlist
    int player_session[MAX_COOKIES];

    // variable to check for a winning game
    int * win = (int *)malloc(sizeof(int));
    *win = 0;

    // variable to count what round the game is on
    int * game_round = (int *)malloc(sizeof(int));
    *game_round = 1;

    // check if both players have seen the win screen
    int win_check[MAX_PLAYERS];
    for (i = 0; i < MAX_PLAYERS; i++) {
      win_check[i] = 0;
    }

    while (1) {

        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i) {
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds)) {
                // create new socket if there is new incoming connection request
                if (i == sockfd) {
                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    if (newsockfd < 0) {
                        perror("accept");
                    }

                    else {
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);

                        // update the maximum tracker
                        if (newsockfd > maxfd) {
                            maxfd = newsockfd;
                        }
                    }
                }
                // a request is sent from the client
                else if (!handle_http_request(i, cookie, win, game_round, players,
                  keyword_list, keyword_count, player_session, count_assignment, win_check,
                  current_players, game_over)) {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }
  }
  return 0;
}

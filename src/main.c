#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "projects/arvelie/arvelie.c"

#include "helpers.c"

#define KEY_BUF_LEN 32
#define STR_BUF_LEN 255
#define DICT_BUFFER 46
#define LIST_BUFFER 46
#define TERM_DICT_BUFFER 16
#define TERM_LIST_BUFFER 16
#define TERM_BODY_BUFFER 24
#define TERM_LINK_BUFFER 8
#define TERM_CHILDREN_BUFFER 16
#define JOURNAL_BUFFER 4000
#define LEXICON_BUFFER 512
#define LOGS_RANGE 56

char *html_head =
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='utf-8'>"
    "<meta name='description' content='%s'/>"
    "<meta name='thumbnail' "
    "content='https://wiki.xxiivv.com/media/services/thumbnail.jpg' />"
    "<link rel='alternate' type='application/rss+xml' title='RSS Feed' "
    "href='../links/rss.xml' />"
    "<link rel='stylesheet' type='text/css' href='../links/main.css'>"
    "<link rel='shortcut icon' type='image/png' "
    "href='../media/services/icon.png'>"
    "<title>XXIIVV — %s</title></head><body>";

char *html_header = "<header>"
                    "<a href='home.html'><img src='../media/icon/logo.svg' "
                    "alt='XXIIVV'></a></header>";

char *html_footer =
    "<footer>"
    "<a href='https://creativecommons.org/licenses/by-nc-sa/4.0'>"
    "<img src='../media/icon/cc.svg'/></a>"
    "<a href='http://webring.xxiivv.com/'>"
    "<img src='../media/icon/rotonde.svg'/>"
    "</a>"
    "<a href='https://merveilles.town/@neauoire'>"
    "<img src='../media/icon/merveilles.svg'/>"
    "</a>"
    "<a href='https://github.com/neauoire'>"
    "<img src='../media/icon/github.png' alt='github'/>"
    "</a>"
    "<span>"
    "<a href='devine_lu_linvega.html'>Devine Lu Linvega</a> © 2020 "
    "— <a href='about.html'>BY-NC-SA 4.0</a>"
    "</span>"
    "</footer></body></html>";

typedef struct List {
  char name[KEY_BUF_LEN];
  char keys[100][100];
  char vals[100][500];
  int pairs_len;
  char items[100][500];
  int items_len;
  int links_len;
} List;

typedef struct Term {
  char name[KEY_BUF_LEN];
  char host[21];
  char bref[200];
  char type[KEY_BUF_LEN];
  char date_from[6];
  char date_last[6];
  char body[30][750];
  int body_len;
  char link_keys[20][KEY_BUF_LEN];
  char link_vals[20][100];
  int link_len;
  char list[20][31];
  int list_len;
  struct Term *parent;
  struct Term *children[20];
  int children_len;
  List *docs[20];
  int docs_len;
  struct Term *incoming[30];
  int incoming_len;
} Term;

typedef struct Log {
  char date[6];
  char rune[1];
  int code;
  char host[KEY_BUF_LEN];
  int pict;
  char name[KEY_BUF_LEN];
  bool is_event;
  Term *term;
} Log;

typedef struct Glossary {
  int len;
  List lists[100];
} Glossary;

typedef struct Lexicon {
  int len;
  Term terms[350];
} Lexicon;

typedef struct Journal {
  int len;
  Log logs[3500];
} Journal;

Glossary all_lists;
Lexicon all_terms;
Journal all_logs;

List *find_list(Glossary *glossary, char *name) {
  int i;
  for (i = 0; i < glossary->len; ++i) {
    List *l = &glossary->lists[i];
    if (!strcmp(name, l->name)) {
      return l;
    }
  }
  return NULL;
}

Term *find_term(Lexicon *lexicon, char *name) {
  int i;
  char buffer[STR_BUF_LEN];
  to_alphanum(name, buffer);
  for (i = 0; i < lexicon->len; ++i) {
    Term *t = &lexicon->terms[i];
    if (!strcmp(buffer, t->name)) {
      return t;
    }
  }
  return NULL;
}

Log *find_last_diary(Term *term) {
  int i;
  for (i = 0; i < all_logs.len; ++i) {
    Log *l = &all_logs.logs[i];
    if (l->term != term || l->pict < 1) {
      continue;
    }
    return l;
  }
  return NULL;
}

void register_incoming(Term *src, char *dest) {
  Term *host = find_term(&all_terms, dest);
  if (host) {
    host->incoming[host->incoming_len] = src;
    host->incoming_len++;
  } else {
    printf("Warning: Unknown incoming(%s)\n", dest);
  }
}

void build_lifeline(FILE *f, Term *term) {
  fputs("<code style='float:right; font-size:80%'>", f);
  fputs_lifeline(f, arvelie_to_epoch("06I04"), get_epoch(),
                 arvelie_to_epoch(term->date_from),
                 arvelie_to_epoch(term->date_last), 6);
  fputs("</code>", f);
}

void build_pict(FILE *f, int pict, char *host, char *name, bool caption,
                char *link) {
  fputs("<figure>", f);
  fprintf(f, "<img src='../media/diary/%d.jpg' alt='%s picture'/>", pict, name);
  if (caption) {
    fputs("<figcaption>", f);
    if (link) {
      fprintf(f, "<a href='%s.html'>%s</a> — %s", link, host, name);
    } else {
      fprintf(f, "%s — %s", host, name);
    }
    fputs("</figcaption>", f);
  }
  fputs("</figure>", f);
}

void build_term_pict(FILE *f, Term *term, bool caption) {
  char filename[STR_BUF_LEN];
  Log *log = find_last_diary(term);
  if (log == NULL) {
    return;
  }
  to_filename(term->name, filename);
  build_pict(f, log->pict, term->name, term->bref, caption, filename);
}

void build_log_pict(FILE *f, Log *log, bool caption) {
  build_pict(f, log->pict, log->date, log->name, caption, NULL);
}

void build_body_part(FILE *f, Term *term) {
  int i;
  for (i = 0; i < term->body_len; ++i) {
    fputs(term->body[i], f);
  }
}

void build_nav_part(FILE *f, Term *term, Term *target) {
  int i;
  fputs("<ul>", f);
  for (i = 0; i < term->children_len; ++i) {
    char child_filename[STR_BUF_LEN];
    to_filename(term->children[i]->name, child_filename);
    if (term->children[i]->name == term->name) {
      continue; /* Paradox */
    }
    if (term->children[i]->name == target->name) {
      fprintf(f, "<li><a href='%s.html'>%s/</a></li>", child_filename,
              term->children[i]->name);
    } else {
      fprintf(f, "<li><a href='%s.html'>%s</a></li>", child_filename,
              term->children[i]->name);
    }
  }
  fputs("</ul>", f);
}

void build_banner(FILE *f, Term *term, bool caption) {
  Log *l = find_last_diary(term);
  if (l) {
    build_log_pict(f, l, caption);
  }
}

void build_nav(FILE *f, Term *term) {
  if (term->parent == NULL) {
    printf("Warning: Missing parent for %s\n", term->name);
    return;
  }
  if (term->parent->parent == NULL) {
    printf("Warning: Missing parent for %s\n", term->parent->name);
    return;
  }
  fputs("<nav>", f);
  if (term->parent->parent->name == term->parent->name) {
    build_nav_part(f, term->parent->parent, term);
  } else {
    build_nav_part(f, term->parent->parent, term->parent);
  }
  if (term->parent->parent->name != term->parent->name) {
    build_nav_part(f, term->parent, term);
  }
  if (term->parent->name != term->name) {
    build_nav_part(f, term, term);
  }
  fputs("</nav>", f);
}

void build_body(FILE *f, Term *term) {
  fprintf(f, "<h2>%s</h2>", term->bref);
  build_body_part(f, term);
}

void build_listing(FILE *f, Term *term) {
  int i, j;
  for (i = 0; i < term->docs_len; ++i) {
    List *l = term->docs[i];
    fprintf(f, "<h3>%s</h3>", l->name);
    fputs("<ul>", f);
    for (j = 0; j < l->pairs_len; ++j) {
      fprintf(f, "<li><b>%s</b>: %s</li>", l->keys[j], l->vals[j]);
    }
    for (j = 0; j < l->items_len; ++j) {
      fprintf(f, "<li>%s</li>", l->items[j]);
    }
    fputs("</ul>", f);
  }
}

void build_include(FILE *f, Term *term) {
  char buffer[4096];
  char filename[STR_BUF_LEN];
  char filepath[STR_BUF_LEN];
  FILE *fp;
  to_filename(term->name, filename);
  filepath[0] = '\0';
  strcat(filepath, "inc/");
  strcat(filepath, filename);
  strcat(filepath, ".htm\0");
  fp = fopen(filepath, "r");
  if (fp == NULL) {
    return;
  }
  for (;;) {
    size_t sz = fread(buffer, 1, sizeof(buffer), fp);
    if (sz) {
      fwrite(buffer, 1, sz, f);
    } else if (feof(fp) || ferror(fp)) {
      break;
    }
  }
  fprintf(f,
          "<p>Found a mistake? Submit an <a "
          "href='https://github.com/XXIIVV/Oscean/edit/master/src/inc/%s.htm' "
          " target='_blank'>edit</a> to %s.</p>",
          term->name, term->name);
  fclose(fp);
}

void build_index(FILE *f, Term *term) {
  int i;
  if (strcmp(term->type, "index") != 0) {
    return;
  }
  for (i = 0; i < term->children_len; ++i) {
    char child_filename[STR_BUF_LEN];
    to_filename(term->children[i]->name, child_filename);
    fprintf(f, "<h3><a href='%s.html'>%s</a></h3>", child_filename,
            term->children[i]->name);
    build_body_part(f, term->children[i]);
    build_listing(f, term->children[i]);
  }
}

void build_portal(FILE *f, Term *term) {
  int i;
  if (strcmp(term->type, "portal") != 0) {
    return;
  }
  for (i = 0; i < term->children_len; ++i) {
    build_term_pict(f, term->children[i], true);
  }
}

void build_album(FILE *f, Term *term) {
  int i;
  Log *header_log;
  if (strcmp(term->type, "album") != 0) {
    return;
  }
  header_log = find_last_diary(term);
  for (i = 0; i < all_logs.len; ++i) {
    Log l = all_logs.logs[i];
    if (l.term != term) {
      continue;
    }
    if (l.pict < 1) {
      continue;
    }
    if (l.pict == header_log->pict) {
      continue;
    }
    build_log_pict(f, &l, true);
  }
}

void build_links(FILE *f, Term *term) {
  int i;
  if (term->link_len < 1) {
    return;
  }
  fputs("<ul>", f);
  for (i = 0; i < term->link_len; ++i) {
    fprintf(f, "<li><a href='%s' target='_blank'>%s</a></li>",
            term->link_vals[i], term->link_keys[i]);
  }
  fputs("</ul>", f);
}

void build_incoming(FILE *f, Term *term) {
  int i;
  char filename[STR_BUF_LEN];
  if (term->incoming_len < 1) {
    return;
  }
  fputs("<p>", f);
  fprintf(f, "<i>incoming(%d)</i>: ", term->incoming_len);
  for (i = 0; i < term->incoming_len; ++i) {
    to_filename(term->incoming[i]->name, filename);
    fprintf(f, "<a href='%s.html'>%s</a> ", filename, term->incoming[i]->name);
  }
  fputs("</p>", f);
}

void build_horaire(FILE *f, Term *term) {
  int i;
  int len = 0;
  int events_len = 0;
  int ch = 0;
  int fh = 0;
  for (i = 0; i < all_logs.len; ++i) {
    Log *l = &all_logs.logs[i];
    if (l->term != term && l->term->parent != term) {
      continue;
    }
    if (l->is_event == true) {
      events_len++;
    }
    ch += (l->code / 10) % 10;
    fh += l->code % 10;
    len++;
  }
  /* Updated */
  if (len < 2 || strlen(term->date_last) == 0) {
    return;
  }
  fputs("<p>", f);
  fprintf(f,
          "<i>"
          "Last update on <a href='tracker.html'>%s</a>, edited %d times. "
          "+%d/%dfh"
          "</i>",
          term->date_last, len, ch, fh);
  build_lifeline(f, term);
  fputs("</p>", f);
  /* Events */
  if (events_len < 1) {
    return;
  }
  fputs("<ul>", f);
  for (i = 0; i < all_logs.len; ++i) {
    Log *l = &all_logs.logs[i];
    if (l->term != term && l->term->parent != term) {
      continue;
    }
    if (l->is_event != true) {
      continue;
    }
    fprintf(f, "<li>%s — %s</li>", l->date, l->name);
  }
  fputs("</ul>", f);
}

void build_special_home(FILE *f, Term *term, Journal *journal) {
  int i;
  bool found_events;
  if (strcmp(term->name, "home") != 0) {
    return;
  }
  found_events = false;
  for (i = 0; i < 5; ++i) {
    if (journal->logs[i].is_event == true) {
      found_events = true;
      break;
    }
  }
  if (!found_events) {
    return;
  }
  fputs("<h2>Events</h2>", f);
  fputs("<ul>", f);
  for (i = 0; i < 5; ++i) {
    char filename[STR_BUF_LEN];
    if (journal->logs[i].is_event != true) {
      continue;
    }
    to_filename(journal->logs[i].term->name, filename);
    fprintf(f, "<li><a href='%s.html'>%s</a> %s</li>", filename,
            journal->logs[i].date, journal->logs[i].name);
  }
  fputs("</ul>", f);
}

void build_special_calendar(FILE *f, Term *term, Journal *journal) {
  int i;
  int last_year;
  if (strcmp(term->name, "calendar") != 0) {
    return;
  }
  last_year = 0;
  fputs("<ul>", f);
  for (i = 0; i < journal->len; ++i) {
    char filename[STR_BUF_LEN];
    if (journal->logs[i].is_event != true) {
      continue;
    }
    if (last_year != extract_year(journal->logs[i].date)) {
      fprintf(f, "</ul><ul>");
    }
    to_filename(journal->logs[i].term->name, filename);
    fprintf(f, "<li><a href='%s.html'>%s</a> %s</li>", filename,
            journal->logs[i].date, journal->logs[i].name);
    last_year = extract_year(journal->logs[i].date);
  }
  fputs("</ul>", f);
}

void build_special_tracker(FILE *f, Term *term, Journal *journal) {
  int i;
  int known_id;
  int last_year;
  if (strcmp(term->name, "tracker") != 0) {
    return;
  }
  known_id = 0;
  last_year = 20;
  fputs("<ul>", f);
  for (i = 0; i < journal->len; ++i) {
    char filename[STR_BUF_LEN];
    char *known[LEXICON_BUFFER];
    if (index_of_string(known, known_id, journal->logs[i].term->name) > -1) {
      continue;
    }
    if (known_id >= LEXICON_BUFFER) {
      printf("Warning: Reached tracker buffer\n");
      break;
    }
    if (last_year != extract_year(journal->logs[i].date)) {
      fprintf(f, "</ul><ul>");
    }
    to_filename(journal->logs[i].term->name, filename);
    fputs("<li>", f);
    fprintf(f, "<a href='%s.html'>%s</a> — last update %s", filename,
            journal->logs[i].term->name, journal->logs[i].date);
    build_lifeline(f, journal->logs[i].term);
    fputs("</li>", f);

    last_year = extract_year(journal->logs[i].date);
    known[known_id] = journal->logs[i].term->name;
    known_id++;
  }
  fputs("</ul>", f);
}

void build_special_journal(FILE *f, Term *term, Journal *journal) {
  int i;
  int count;
  if (strcmp(term->name, "journal") != 0) {
    return;
  }
  count = 0;
  for (i = 0; i < journal->len; ++i) {
    if (count > 20) {
      break;
    }
    if (journal->logs[i].pict == 0) {
      continue;
    }
    build_log_pict(f, &journal->logs[i], true);
    count++;
  }
}

void build_special_now(FILE *f, Term *term, Journal *journal) {
  time_t now;
  int past_len = 0;
  float past_value[LOGS_RANGE];
  char *past_name[LOGS_RANGE];
  float past_sum_value = 0;
  char filename[STR_BUF_LEN];
  int projects_len = 0;
  float projects_value[LOGS_RANGE];
  char *projects_name[LOGS_RANGE];
  float sum_value = 0;
  float test_sum;
  float ratio;
  int past_index;
  int i;
  Log l;
  int index;
  if (strcmp(term->name, "now") != 0) {
    return;
  }
  for (i = 0; i < LOGS_RANGE; ++i) {
    past_index = i + LOGS_RANGE;
    l = journal->logs[past_index];
    if (l.code % 10 < 1) {
      continue;
    }
    index = index_of_string(past_name, past_len, l.term->name);
    if (index < 0) {
      past_name[past_len] = l.term->name;
      past_value[past_len] = 0;
      past_len++;
    }
    past_value[past_len - 1] += l.code % 10;
    past_sum_value += l.code % 10;
  }

  for (i = 0; i < LOGS_RANGE; ++i) {
    l = journal->logs[i];
    if (l.code % 10 < 1) {
      continue;
    }
    index = index_of_string(projects_name, projects_len, l.term->name);
    if (index < 0) {
      projects_name[projects_len] = l.term->name;
      projects_value[projects_len] = 0;
      projects_len++;
    }
    projects_value[projects_len - 1] += l.code % 10;
    sum_value += l.code % 10;
  }

  fprintf(
      f,
      "<p>Distribution of <b>%.0f hours over %d projects</b>, a change of %.0f "
      "hours and %d projects since the previous period of %d days.</p>",
      sum_value, projects_len, sum_value - past_sum_value,
      projects_len - past_len, LOGS_RANGE);

  test_sum = 0;
  fputs("<ul style='columns:2'>", f);
  for (i = 0; i < projects_len; ++i) {
    to_filename(projects_name[i], filename);
    ratio = (projects_value[i] / sum_value) * 100;
    past_index = index_of_string(past_name, past_len, projects_name[i]);
    fputs("<li>", f);
    fprintf(f, "<a href='%s.html'>%s</a> %.2f&#37; ", filename,
            projects_name[i], ratio);
    if (past_index >= 0) {
      float past_ratio = (past_value[past_index] / past_sum_value) * 100;
      float diff = ratio - past_ratio;
      test_sum += diff;
      if (diff > 0) {
        fprintf(f, "<i style='color:#42ae92'>+%.1f&#37;</i>", diff);
      } else {
        fprintf(f, "<i style='color:red'>%.1f&#37;</i>", diff);
      }
    }
    fputs("</li>", f);
  }
  fputs("</ul>", f);
  time(&now);
  fprintf(f, "<p>Last generated on %s(Japan).</p>", ctime(&now));
}

void print_term_details(FILE *f, Term *term, int depth) {
  int i;
  char filename[STR_BUF_LEN];
  depth++;
  to_filename(term->name, filename);
  fprintf(f, "<li><a href='%s.html'>%s</a></li>", filename, term->name);
  if (term->children_len < 1) {
    return;
  }
  fputs("<ul>", f);
  for (i = 0; i < term->children_len; ++i) {
    if (strcmp(term->children[i]->name, term->name) != 0) {
      print_term_details(f, term->children[i], depth);
    }
  }
  fputs("</ul>", f);
}

void build_special_index(FILE *f, Term *term) {
  if (strcmp(term->name, "index") != 0) {
    return;
  }
  fputs("<ul>", f);
  print_term_details(f, &all_terms.terms[0], 0);
  fputs("</ul>", f);
}

void build_page(Term *term, Journal *journal) {
  FILE *f;
  char filename[STR_BUF_LEN];
  char filepath[STR_BUF_LEN];
  to_filename(term->name, filename);
  filepath[0] = '\0';
  strcat(filepath, "../site/");
  strcat(filepath, filename);
  strcat(filepath, ".html\0");
  f = fopen(filepath, "w");
  fprintf(f, html_head, term->bref, term->name);
  fputs(html_header, f);
  build_nav(f, term);
  fputs("<main>", f);
  build_banner(f, term, true);
  build_body(f, term);
  build_include(f, term);
  build_listing(f, term);
  build_index(f, term);
  build_portal(f, term);
  build_album(f, term);
  build_links(f, term);
  build_incoming(f, term);
  build_horaire(f, term);
  build_special_home(f, term, journal);
  build_special_calendar(f, term, journal);
  build_special_tracker(f, term, journal);
  build_special_journal(f, term, journal);
  build_special_now(f, term, journal);
  build_special_index(f, term);
  fputs("</main>", f);
  fputs(html_footer, f);
  fclose(f);
}

void build_rss(Journal *journal) {
  int i;
  FILE *f = fopen("../links/rss.xml", "w");
  fputs("<?xml version='1.0' encoding='UTF-8' ?>", f);
  fputs("<rss version='2.0' xmlns:dc='http://purl.org/dc/elements/1.1/'>", f);
  fputs("<channel>\n", f);
  fputs("<title>XXIIVV — Oscean</title>\n", f);
  fputs("<link><![CDATA[https://wiki.xxiivv.com/Journal]]></link>\n", f);
  fputs("<description>The Nataniev Library</description>\n", f);
  for (i = 0; i < journal->len; ++i) {
    char filename[STR_BUF_LEN];
    Log l = journal->logs[i];
    if (l.pict == 0) {
      continue;
    }
    to_filename(l.term->name, filename);
    fputs("<item>\n", f);
    fprintf(f, "  <title>%s</title>\n", l.name);
    fprintf(f, "  <link>https://wiki.xxiivv.com/site/%s.html</link>\n",
            filename);
    fprintf(f, "  <guid isPermaLink='false'>%d</guid>\n", l.pict);
    fputs("  <pubDate>", f);
    fputs_rfc2822(f, l.date);
    fputs("</pubDate>\n", f);
    fputs("  <dc:creator><![CDATA[Devine Lu Linvega]]></dc:creator>\n", f);
    fputs("  <description>\n", f);
    fputs("<![CDATA[", f);
    fprintf(f, "<img src='https://wiki.xxiivv.com/media/diary/%d.jpg'/>\n",
            l.pict);
    fprintf(f,
            "<p>%s<br/><br/><a "
            "href='https://wiki.xxiivv.com/site/%s.html'>%s</a></p>",
            l.term->bref, filename, l.term->name);
    fputs("]]>\n", f);
    fputs("  </description>\n", f);
    fputs("</item>\n", f);
  }
  fputs("</channel>", f);
  fputs("</rss>", f);
  fclose(f);
}

FILE *parse_glossary(FILE *fp, Glossary *glossary) {
  int key_len;
  int val_len;
  int len, pad, count = 0;
  char line[512];
  while (fgets(line, 512, fp)) {
    pad = count_leading_spaces(line);
    trimstr(line);
    len = strlen(line);
    if (len < 3 || line[0] == ';') {
      continue;
    }
    if (len > 400) {
      printf("Warning: Line is too long(%d characters) %s\n", len, line);
      continue;
    }
    if (pad == 0) {
      List *l = &glossary->lists[glossary->len];
      substr(line, l->name, 0, len);
      lcstr(l->name);
      glossary->len++;
    } else if (pad == 2) {
      List *l = &glossary->lists[glossary->len - 1];
      if (strstr(line, " : ") != NULL) {
        key_len = index_of_char(line, ':') - 3;
        substr(line, l->keys[l->pairs_len], 2, key_len);
        val_len = len - key_len - 5;
        substr(line, l->vals[l->pairs_len], key_len + 5, val_len);
        l->pairs_len++;
      } else {
        substr(line, l->items[l->items_len], 2, len);
        l->items_len++;
      }
    }
    count++;
  }
  printf("(%d lines) ", count);
  return fp;
}

FILE *parse_lexicon(FILE *fp, Lexicon *lexicon) {
  char line[1024];
  int key_len;
  int val_len;
  int len, count = 0;
  bool catch_body = false;
  bool catch_link = false;
  bool catch_list = false;
  while (fgets(line, 1024, fp)) {
    int pad = count_leading_spaces(line);
    trimstr(line);
    len = strlen(line);
    if (len < 3 || line[0] == ';') {
      continue;
    }
    if (len > 750) {
      printf("Warning: Line is too long(%d characters): %s \n", len, line);
      continue;
    }
    if (pad == 0) {
      Term *t = &lexicon->terms[lexicon->len];
      substr(line, t->name, 0, len);
      lcstr(t->name);
      lexicon->len++;
    } else if (pad == 2) {
      Term *t = &lexicon->terms[lexicon->len - 1];
      if (strstr(line, "HOST : ") != NULL) {
        substr(line, t->host, 9, len - 9);
      }
      if (strstr(line, "BREF : ") != NULL) {
        substr(line, t->bref, 9, len - 9);
      }
      if (strstr(line, "TYPE : ") != NULL) {
        substr(line, t->type, 9, len - 9);
      }
      catch_body = strstr(line, "BODY") != NULL;
      catch_link = strstr(line, "LINK") != NULL;
      catch_list = strstr(line, "LIST") != NULL;
    } else if (pad == 4) {
      Term *t = &lexicon->terms[lexicon->len - 1];
      /* Body */
      if (catch_body) {
        substr(line, t->body[t->body_len], 4, len - 4);
        t->body_len++;
      }
      /* Link */
      if (catch_link) {
        key_len = index_of_char(line, ':') - 5;
        substr(line, t->link_keys[t->link_len], 4, key_len);
        val_len = len - key_len - 5;
        substr(line, t->link_vals[t->link_len], key_len + 7, val_len);
        t->link_len++;
      }
      /* List */
      if (catch_list) {
        substr(line, t->list[t->list_len], 4, len - 4);
        t->list_len++;
      }
    }
    count++;
  }
  printf("(%d lines) ", count);
  return fp;
}

FILE *parse_horaire(FILE *fp, Journal *journal) {
  int len, count = 0;
  char line[STR_BUF_LEN];
  char codebuff[4];
  Log *l;
  while (fgets(line, STR_BUF_LEN, fp)) {
    trimstr(line);
    len = strlen(line);
    if (len < 14 || line[0] == ';') {
      continue;
    }
    if (len > 72) {
      printf("Warning: Entry is too long %s\n", line);
      continue;
    }
    l = &journal->logs[journal->len];
    /* Date */
    substr(line, l->date, 0, 5);
    /* Rune */
    substr(line, l->rune, 6, 1);
    l->is_event = !strcmp(l->rune, "+");
    /* Code */
    substr(line, codebuff, 7, 3);
    l->code = atoi(codebuff);
    /* Term */
    substr(line, l->host, 11, 21);
    trimstr(l->host);
    /* Pict */
    if (len >= 35) {
      char pictbuff[4];
      substr(line, pictbuff, 32, 3);
      l->pict = atoi(pictbuff);
    }
    /* Name */
    if (len >= 38) {
      substr(line, l->name, 36, 30);
      trimstr(l->name);
    }
    journal->len++;
    count++;
  }
  printf("(%d lines) ", count);
  return fp;
}

void parse(void) {
  printf("Parsing  | ");
  printf("glossary");
  fclose(parse_glossary(fopen("database/glossary.ndtl", "r"), &all_lists));
  printf("lexicon");
  fclose(parse_lexicon(fopen("database/lexicon.ndtl", "r"), &all_terms));
  printf("horaire");
  fclose(parse_horaire(fopen("database/horaire.tbtl", "r"), &all_logs));
}

void link(void) {
  int i, j;
  printf("Linking  | ");
  printf("journal(%d entries) ", all_logs.len);
  for (i = 0; i < all_logs.len; ++i) {
    Log *l = &all_logs.logs[i];
    l->term = find_term(&all_terms, l->host);
    if (!l->term) {
      printf("Warning: Unknown log host %s\n", l->host);
    } else {
      if (strlen(l->term->date_last) == 0) {
        cpystr(l->date, l->term->date_last);
      }
      cpystr(l->date, l->term->date_from);
    }
  }
  printf("lexicon(%d entries) ", all_terms.len);
  for (i = 0; i < all_terms.len; ++i) {
    Term *t = &all_terms.terms[i];
    t->parent = find_term(&all_terms, t->host);
    if (!t->parent) {
      printf("Warning: Unknown term host %s\n", t->host);
    }
    t->parent->children[t->parent->children_len] = t;
    t->parent->children_len++;
  }
  printf("glossary(%d entries) ", all_lists.len);
  for (i = 0; i < all_terms.len; ++i) {
    Term *t = &all_terms.terms[i];
    for (j = 0; j < t->list_len; ++j) {
      List *l = find_list(&all_lists, t->list[j]);
      if (!l) {
        printf("Warning: Unknwon list \"%s\" for %s\n", t->list[j], t->name);
        continue;
      }
      t->docs[t->docs_len] = l;
      t->docs_len++;
      l->links_len++;
    }
  }
}

void template_mods(char *src, char *dest) {
  int split = index_of_char(src, ' ');
  char target[256];
  substr(src, target, split + 1, strlen(src) - split - 2);
  /* create new string */
  dest[0] = '\0';
  if (strstr(src, "^itchio") != NULL) {
    strcat(dest, "<iframe frameborder='0' src='https://itch.io/embed/");
    strcat(dest, target);
    strcat(dest, "?link_color=000000' width='600' height='167'></iframe>");
  } else if (strstr(src, "^bandcamp") != NULL) {
    strcat(dest, "<iframe style='border: 0; width: 600px; height: 274px;' ");
    strcat(dest, "src='https://bandcamp.com/EmbeddedPlayer/album=");
    strcat(dest, target);
    strcat(dest, "/size=large/bgcol=ffffff/linkcol=333333/artwork=small' ");
    strcat(dest, "seamless></iframe>");
  } else if (strstr(src, "^youtube") != NULL) {
    strcat(dest, "<iframe width='600' height='380' ");
    strcat(dest, "src='https://www.youtube.com/embed/");
    strcat(dest, target);
    strcat(dest, "?rel=0' style='max-width:700px' frameborder='0' ");
    strcat(dest, "allow='autoplay; encrypted-media' allowfullscreen></iframe>");
  } else if (strstr(src, "^redirect") != NULL) {
    strcat(dest, "<meta http-equiv='refresh' content='2; url=");
    strcat(dest, target);
    strcat(dest, ".html' />");
    strcat(dest, "<p>In a hurry? Travel to <a href='");
    strcat(dest, target);
    strcat(dest, ".html'>");
    strcat(dest, target);
    strcat(dest, "</a>.</p>");
  } else {
    printf("Warning: Missing template mod: %s\n", src);
  }
}

void template_link(char *src, char *dest) {
  int split = index_of_char(src, ' ');
  char target[256], name[256];
  /* find target and name */
  if (split == -1) {
    substr(src, target, 1, strlen(src) - 2);
    cpystr(target, name);
  } else {
    substr(src, target, 1, split - 1);
    substr(src, name, split + 1, strlen(src) - split - 2);
  }
  /* create new string */
  dest[0] = '\0';
  if (is_url(target)) {
    strcat(dest, "<a href='");
    strcat(dest, target);
    strcat(dest, "' target='_blank'>");
    strcat(dest, name);
    strcat(dest, "</a>");
  } else {
    if (!find_term(&all_terms, target)) {
      printf("Warning: Unknown link(%s:%s)\n", target, name);
      strcat(dest, target);
    } else {
      strcat(dest, "<a href='");
      to_filename(target, target);
      strcat(dest, target);
      strcat(dest, ".html'>");
      strcat(dest, name);
      strcat(dest, "</a>");
    }
  }
}

void template_seg(Term *term, char *src) {
  bool recording = false;
  char buffer[512], fw[512], full[512], res[1024], templated[1024];
  int i, len = strlen(src);
  cpystr(src, res);
  for (i = 0; i < len; ++i) {
    char c = src[i];
    if (c == '}') {
      recording = false;
      /* capture full template */
      substr(src, full, i - strlen(buffer) - 1, strlen(buffer) + 2);
      if (full[1] == '^') {
        template_mods(full, templated);
      } else {
        template_link(full, templated);
      }
      replacesubstr(res, res, full, templated);
      /* save incoming */
      firstword(buffer, fw);
      if (!is_url(fw) && fw[0] != '^') {
        register_incoming(term, fw);
      }
    }
    if (recording) {
      chrcat(buffer, c);
    }
    if (c == '{') {
      recording = true;
      buffer[0] = '\0';
    }
  }
  cpystr(res, src);
}

bool req_template(char *str) {
  int i, open = 0, shut = 0, len = strlen(str);
  for (i = 0; i < len; i++) {
    if (str[i] == '{') {
      open++;
    } else if (str[i] == '}') {
      shut++;
    }
  }
  if (open != shut) {
    printf("Warning: Templating mismatch: %s(%d/%d)\n", str, open, shut);
  }
  return open > 0 && shut > 0;
}

void template(void) {
  int i, j, count = 0;
  printf("Template | ");
  for (i = 0; i < all_terms.len; ++i) {
    Term *term = &all_terms.terms[i];
    for (j = 0; j < term->body_len; ++j) {
      if (req_template(term->body[j])) {
        template_seg(term, term->body[j]);
        count++;
      }
    }
  }
  printf("%d strings", count);
}

void build(void) {
  int i;
  printf("Building | ");
  printf("%d pages ", all_terms.len);
  for (i = 0; i < all_terms.len; ++i) {
    build_page(&all_terms.terms[i], &all_logs);
  }
  printf("1 feed ");
  build_rss(&all_logs);
}

void check(void) {
  int pict_used_len;
  int pict_used[999];
  int i;
  pict_used_len = 0;
  printf("Checking | ");
  /* Find invalid logs */
  for (i = 0; i < all_logs.len; ++i) {
    Log *l = &all_logs.logs[i];
    if (l->code < 1) {
      printf("Warning: Empty code %s\n", l->date);
    }
    if (l->pict > 0) {
      pict_used[pict_used_len] = l->pict;
      pict_used_len++;
    }
  }
  /* Find unlinked lists */
  for (i = 0; i < all_lists.len; ++i) {
    List *l = &all_lists.lists[i];
    if (l->links_len < 1) {
      printf("Warning: Unlinked list \"%s\"\n", l->name);
    }
  }
  /* Find next available diary id */
  for (i = 1; i < 999; ++i) {
    int index = index_of_int(pict_used, pict_used_len, i);
    if (index < 0) {
      printf("Completed at #%d ", i);
      break;
    }
  }
}

int main(void) {
  clock_t start;

  printf("Today    | ");
  print_arvelie();

  start = clock();
  parse();
  printf("[%.2fms]\n", clock_since(start));
  start = clock();
  link();
  printf("[%.2fms]\n", clock_since(start));
  start = clock();
  template();
  printf("[%.2fms]\n", clock_since(start));
  start = clock();
  build();
  printf("[%.2fms]\n", clock_since(start));
  start = clock();
  check();
  printf("[%.2fms]\n", clock_since(start));

  return (0);
}

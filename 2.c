#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define IN_LINE_COMMENT 1
#define IN_BLOCK_COMMENT 2
#define IN_STRING 4
#define IN_ID 8

#define MAX_ID_LEN 64
#define MAX_IDS 1024
#define HASHMAP_SIZE 4019

#define TEST 0  // 1 - dla testowania,  0 - dla automatycznej oceny
// #define PRINT // uncomment before define to print each ident

int index_cmp(const void*, const void*);
int cmp(const void*, const void*);

char tab[MAX_IDS][MAX_ID_LEN];

static int hashmap_is_init = 0;
const char *hashmap[HASHMAP_SIZE] = { NULL };

char *keywords[] = 
{
	"auto", "break", "case", "char",
	"const", "continue", "default", "do",
	"double", "else", "enum", "extern",
	"float", "for", "goto", "if",
	"int", "long", "register", "return",
	"short", "signed", "sizeof", "static",
	"struct", "switch", "typedef", "union",
	"unsigned", "void", "volatile", "while"
};

unsigned long
hashmap_djb2(const char *str)
{
	unsigned long h = 5381;
	char c;
	while ((c = *str++))
		h = (h<<5) + h + c;

	return h;
}

void
hashmap_add(const char *str)
{
	unsigned long i = hashmap_djb2(str) % HASHMAP_SIZE;
	unsigned long c = 1;

	while (hashmap[i] != NULL)
	{
		i += c * c;
		c++;
	}

	hashmap[i] = str;
}

int
hashmap_find(const char *str)
{
	unsigned long i = hashmap_djb2(str) % HASHMAP_SIZE;
	unsigned long c = 1;

	while (hashmap[i] != NULL)
	{
		if (strncmp(str, hashmap[i], MAX_ID_LEN) == 0) return 1;
		i += c * c;
		c++;
	}

	return 0;
}

enum parse_mode
{
	NORM,
	COMM_START,
	LINE_COMM,
	BLK_COMM,
	STRING,
	CHAR,
	NUMBER,
	HEADER,
	SKIP,
	BUF_OVFL
};

struct parse_state
{
	char lc;
	char buf[MAX_ID_LEN];
	unsigned char buf_off;
	enum parse_mode mode;
};

void
state_to_norm(struct parse_state *s)
{
	s->mode = NORM;
	s->buf_off = 0;
}

#define IS_NUM(C) (C >= '0' && C <= '9')
#define IS_IDENT_START(C) ( \
		(C >= 'a' && C <= 'z') || \
		(C >= 'A' && C <= 'Z') || \
		C == '_')
#define IS_IDENT(C) (IS_IDENT_START(C) || IS_NUM(C))


int
parse_state(char c, struct parse_state *s)
{
	switch (s->mode)
	{
		case NORM:
			switch (c)
			{
				case '/':
					s->mode = COMM_START;
					break;
				case '"':
					s->mode = STRING;
					break;
				case '\'':
					s->mode = CHAR;
					break;
				case '#':
					s->mode = HEADER;
					s->buf_off = 0; // reset buf for directive label
					break;
				default:
					break;
			}
			
			if (s->buf_off + 1 >= MAX_ID_LEN)
			{
				s->mode = BUF_OVFL;
				return 0;
			}

			if (s->mode != NORM) break;

			if (s->buf_off == 0 && !IS_IDENT_START(c)) {
				if (IS_NUM(c)) s->mode = NUMBER;
				break;
			}
			
			if (!IS_IDENT(c))
			{
				s->buf[s->buf_off] = '\0';
				
				int size = s->buf_off + 1;
				s->buf_off = 0; // reset buf for next token
				
				char *p = malloc(size);
				if (p == NULL) return 0;
				memcpy(p, s->buf, size);

				if (!hashmap_find(s->buf)) 
				{
					#ifdef PRINT 
					puts(p);
					#endif
					
					hashmap_add(p);
					return 1;
				}
			}
			else
			{
				s->buf[s->buf_off++] = c;
			}

			break;

		case COMM_START:
			switch (c)
			{
				case '/': 
					s->mode = LINE_COMM;
					break;
			 	case '*':
					s->mode = BLK_COMM;
					break;
				default: // not a comment
					state_to_norm(s);
					break;
			}	
			break;

		case LINE_COMM:
		case SKIP:
			if (c == '\n' || c == '\r') state_to_norm(s);
			break;

		case BLK_COMM:
			if (s->lc == '*' && c == '/') state_to_norm(s);
			break;

		case STRING:
			if (s->lc != '\\' && c == '"') state_to_norm(s);
			break;
		
		case CHAR:
			if (s->lc != '\\' && c == '\'') state_to_norm(s);
			break;

		case HEADER: // does not skip - will register unused defines
			if (s->buf_off + 1 >= MAX_ID_LEN)
			{
				s->mode = BUF_OVFL;
				return 0;
			}

			if (!IS_IDENT_START(c))
			{
				s->buf[s->buf_off] = '\0';
				int has_ident =
					strncmp(s->buf, "define", MAX_ID_LEN) == 0 ||
					strncmp(s->buf, "ifndef", MAX_ID_LEN) == 0 ||
					strncmp(s->buf, "ifdef", MAX_ID_LEN) == 0;

				if (has_ident) state_to_norm(s);
				else s->mode = SKIP;
			}
			else
			{
				s->buf[s->buf_off++] = c;
			}
			break;

		case NUMBER:
		case BUF_OVFL:
			if (!IS_IDENT(c)) state_to_norm(s);
			break;
	}

	s->lc = c;

	return 0;
}

int 
find_idents(FILE *stream)
{
	// init hashmap
	if (!hashmap_is_init)
	{
		for (int i = 0; i < sizeof(keywords) / sizeof(char *); i++)
			hashmap_add(keywords[i]);
		hashmap_is_init = 1;
	}

	char c;

	struct parse_state state = 
	{ 
		.lc = '\0', 
		.buf = { 0 }, 
		.buf_off = 0, 
		.mode = NORM 
	};

	unsigned int id_cnt = 0;

	while ((c = fgetc(stream)) != EOF)
		id_cnt += parse_state(c, &state);

	return id_cnt;
}

int 
cmp(const void* first_arg, const void* second_arg) 
{
	char *a = *(char**)first_arg;
	char *b = *(char**)second_arg;
	return strcmp(a, b);
}

int 
index_cmp(const void* first_arg, const void* second_arg) 
{
	int a = *(int*)first_arg;
	int b = *(int*)second_arg;
	return strcmp(tab[a], tab[b]);
}

int 
main(void) 
{
	char file_name[40];
	FILE *stream;

	if (TEST) stream = stdin;
	else 
	{
      		scanf("%s",file_name);
		stream = fopen(file_name,"r");
		if (stream == NULL) 
		{
			printf("fopen failed\n");
			return -1;
      		}
  	}
	printf("%d\n", find_idents(stream));
	return 0;
}


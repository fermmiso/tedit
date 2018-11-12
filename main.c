/* FEATURE MACROS */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define GNU_SOURCE

/* INCLUDES */ /* TODO probably make a header file */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

/* DEFINITIONS */
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define CTRL_KEY(k) ((k) & 0x1f) 	
#define BUFFER_CONSTR { NULL, 0 }
#define TEDIT_VERSION "0.1"
#define TEDIT_QUIT 3
#define TAB_STOP 8
#define HIGH_LIGHT_NUMBERS (1<<0)	
#define HIGH_LIGHT_STRINGS (1<<1)

/* PROTOTYPE */
void Refresh_Screen();
void Disable_Raw_Mode();
void Set_Status_Message( onst char *fmt, ...);
char *Prompt(char *prompt, void (*callback)(char *, int));

/* DATA */
enum KEYS{
	BACK_SPACE = 127,
	ARROW_UP = 1000,
	ARROW_DOWN,
	ARROW_LEFT,
	ARROW_RIGHT,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

enum HIGHLIGHT{
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD_1,
	HL_KEYWORD_2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH
};

typedef struct File_row {
	int *idx;
	int *hl_open_comment;
	int *size;
	int *render_size;
	char *render;
	char *string;
	unsigned char *high_lighted;
} File_row;

struct Buffer {
	char *string;
	int length;
};

struct Config {
	int *cursor_x;
	int *cursor_y;
	int *screen_cols;
	int *screen_rows;
	int *num_of_rows;
	int *current_row;
	int *current_col;
	int *render_x;
	int *dirty_flag;
	char *status_msg;
	char *filename;
	time_t status_time;
	File_row *row;
	struct Syntax *syntax;
	struct termios *orig;
};

struct Syntax{
	char *file_type;
	char **file_match;
	char **key_words;
	char *single_line_comment_start;
	char *multi_line_comment_start;
	char *multi_line_comment_end;
	int flags;
};

/* FILE TYPES */
char *C_HL_Extensions[] = {".c",".h",".cpp",NULL};

char *C_HL_Keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

struct Syntax HLDB[] = {
	{
	"c",
	C_HL_Extensions,
	C_HL_Keywords,
	"//","/*","*/",
	HIGH_LIGHT_NUMBERS | HIGH_LIGHT_STRINGS
	},
};
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/* GLOBAL VARIABLES */
struct Config *config;
struct termios *raw;

/* TERMINAL */
void die( const char *string )
{
	Refresh_Screen();
	perror(string);
	exit(1);
}

void free_mem( void *ptr , const char *msg )
{
	if(ptr){
		free(ptr);
	}else{
		printf("Not allocated : %s\n",msg); 
	}
}

void Check_Mem( void *mem, const char *string )
{
	if(!mem){
		printf("Memory error: %s",string);
	}
}

void Alloc_Config()
{
	config = malloc(sizeof(struct Config));	
	Check_Mem(config,"config");
		
	config->orig = malloc(sizeof(struct termios));
	Check_Mem(config->orig,"config->orig");	

	config->screen_cols = malloc(sizeof(int));
	Check_Mem(config->screen_cols,"config->screen_cols");	

	config->screen_rows = malloc(sizeof(int));
	Check_Mem(config->screen_rows,"config->screen_rows");

	config->cursor_x = malloc(sizeof(int));
	Check_Mem(config->cursor_x,"config->cursor_x");	

	config->cursor_y = malloc(sizeof(int));
	Check_Mem(config->cursor_y,"config->cursor_y");	

	config->num_of_rows = malloc(sizeof(int));
	Check_Mem(config->num_of_rows,"config->num_of_rows");

	config->current_row = malloc(sizeof(int));
	Check_Mem(config->current_row,"config->current_row");

	config->current_col = malloc(sizeof(int));
	Check_Mem(config->current_col,"config->current_col");
	
	config->render_x = malloc(sizeof(int));
	Check_Mem(config->render_x,"config->render_x");
	
	config->status_msg = malloc(sizeof(char) * 80);
	Check_Mem(config->status_msg, "config->status_msg");
	
	config->dirty_flag = malloc(sizeof(int));
	Check_Mem(config->dirty_flag, "config->dirty_flag");
	
	/**	config->filename allocated using a strdup in Open_file()	**/
	/**	config->row is allocates in Insert_Row				**/
}

void Free_Rows()
{
	int index = *config->num_of_rows;
	while (index--){
		if(config->row[index].hl_open_comment){
			free_mem(config->row[index].hl_open_comment, "config->row.hl_open_comment");
		}
		if(config->row[index].idx){
			free_mem(config->row[index].idx, "config->row.idx");
		}
		if(config->row[index].high_lighted){
			free_mem(config->row[index].high_lighted, "config->row.high_lighted");
		}
		if(config->row[index].render){
			free_mem(config->row[index].render,"config->row.render");
		}
		if(config->row[index].render_size){
			free_mem(config->row[index].render_size,"config->row->render.size");
		}
		if(config->row[index].string){
			free_mem(config->row[index].string,"config->row->string");
		}
		if(config->row[index].size){
			free_mem(config->row[index].size,"config->row.size");
		}
	}
	if(config->row){
		free_mem(config->row,"config->row");
	}
}

void Disable_Raw_Mode()
{
	if(tcsetattr(STDIN,TCSAFLUSH,config->orig) == -1){
		die("Disable_Raw_mode");
	}
	Free_Rows();		

	if(config->dirty_flag){
		free_mem(config->dirty_flag, "dirty_flag");
	}
	if(config->status_msg){
		free_mem(config->status_msg,"status_msg");
	}
	if(config->filename){
		free_mem(config->filename,"filename");
	}
	if(config->render_x){
		free_mem(config->render_x,"render_x");
	}
	if(config->current_col){
		free_mem(config->current_col,"current_col");
	}
	if(config->current_row){
		free_mem(config->current_row,"current_row");
	}
	if(config->num_of_rows){
		free_mem(config->num_of_rows,"num_of_rows");
	}
	if(config->cursor_x){
		free_mem(config->cursor_x,"cursor_x");
	}
	if(config->cursor_y){
		free_mem(config->cursor_y,"cursor_y");
	}
	if(config->screen_cols){
		free_mem(config->screen_cols,"screen_cols");
	}
	if(config->screen_rows){
		free_mem(config->screen_rows,"screen_rows");
	}
	if(config->orig){
		free_mem(config->orig,"orig");
	}
	if(config){
		free_mem(config,"config");
	}
	if(raw){
		free_mem(raw,"raw");
	}
}	

void Enable_Raw_Mode()
{
	Alloc_Config();
	raw = malloc(sizeof(struct termios));	

	if(tcgetattr(STDIN, config->orig)== -1){
		die("Enable_Raw_Mode get orig");
	}

	memcpy(raw,config->orig,sizeof(struct termios));
	atexit(Disable_Raw_Mode);					
	
	raw->c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP| IXON);	
	raw->c_oflag &= ~(OPOST);
	raw->c_cflag |= ~(CS8);
	raw->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);				
	raw->c_cc[VMIN] = 0;
	raw->c_cc[VTIME] = 1;

	if(tcsetattr(STDIN,TCSAFLUSH, raw) == -1)
	{
		die("Enable_Raw_Mode set");
	}
}

int Read_Key()
{
	int check = 0;
	char key_press = '\0';
	while((check = read(STDIN,&key_press,1))!= 1){
		if(check == -1 && errno != EAGAIN){
			printf("Read");
		}
	}
	if(key_press == '\x1b'){
		char seq[3];
		if(read(STDIN,&seq[0],1)!= 1){
			return '\x1b';		
		}
		if(read(STDIN,&seq[1],1)!= 1){
			return '\x1b';		
		}
		if(seq[0] == '['){
			if(seq[1] >= '0' && seq[1] <= '9'){
				if(read(STDIN,&seq[2],1) != 1){
					return '\x1b';				
				}
				if(seq[2] == '~'){
					switch(seq[1]){
						case '1':
							return HOME_KEY;
						case '3':
							return DEL_KEY;
						case '4':
							return END_KEY;
						case '5':
							return PAGE_UP;
						case '6':
							return PAGE_DOWN;
						case '7':
							return HOME_KEY;
						case '8':
							return END_KEY;
					}
				}		
			}else{
				switch(seq[1]){
					case 'A':
						return ARROW_UP;
					case 'B':
						return ARROW_DOWN;
					case 'C':
						return ARROW_RIGHT;
					case 'D':
						return ARROW_LEFT;
					case 'H':
						return HOME_KEY;
					case 'F':
						return END_KEY;
				}
			}
		}else if(seq[0] == 'O'){
			switch(seq[1]){
				case 'H':
					return HOME_KEY;
				case 'F':
					return END_KEY;
			}
		}
		return '\x1b';
	}else{
		return key_press;
	}
}
	
int Get_Cursor_Position( int *cols, int *rows )
{
	char buffer[32];
	unsigned int i = 0;

	if(write(STDOUT,"\x1b[6n",4) != 4){
		return -1;
	}
	printf("\r\n");
	for(i = 0; i < sizeof(buffer); i++){
		if(read(STDIN,&buffer[i],1) != 1){
			break;
		}
		if(buffer[i] == 'R'){
			break;
		}
	}	

	buffer[i] = '\0';

	if(buffer[0] != '\x1b' && buffer[1] != '['){
		return -1;	
	}
	if(sscanf(&buffer[2],"%d;%d",rows,cols) != 2){
		return -1;
	}
	return 0;	
}

int Get_Win_Size( int *cols, int *rows )
{
	struct winsize *win;
	win  = malloc(sizeof(struct winsize));
	if(ioctl(STDOUT,TIOCGWINSZ,win)== -1 || win->ws_col == 0){
		if(write(STDOUT,"\x1b[999C\x1b[999B",12) != 12){
			return -1;
		}
		free_mem(win,"win");
		return Get_Cursor_Position(cols, rows);
	}else{
		*rows = win->ws_row;
		*cols = win->ws_col;
	}
	free_mem(win,"win_2");
	return 0;
}

/* SYNTAX HIGHLIGHTING */
int Is_Seperator( int c )
{
	return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];",c) != NULL;
}

void Update_Syntax( File_row *row ) /*TODO Break this up into smaller functions.*/
{
	row->high_lighted = realloc(row->high_lighted, *row->render_size);	
	memset(row->high_lighted, HL_NORMAL, *row->render_size);		

	if(config->syntax == NULL){						
		return;
	}
	char **keywords = config->syntax->key_words;				

	char *scs = config->syntax->single_line_comment_start;			
	char *mlcs = config->syntax->multi_line_comment_start;
	char *mlce = config->syntax->multi_line_comment_end;
	
	int scs_len = scs ? strlen(scs) : 0;					
	int mlcs_len = mlcs ? strlen(mlcs): 0;
	int mlce_len = mlce ? strlen(mlce): 0;

	int prev_sep = 1;							
	int in_string = 0;
	int in_comment = (*row->idx > 0 && *config->row[*row->idx - 1].hl_open_comment);

	int i = 0;
	while( i < *row->render_size){
		char c = row->render[i];						
		unsigned char prev_hl = (i > 0) ? row->high_lighted[i - 1] : HL_NORMAL;		
		if(scs_len && !in_string && !in_comment){							
			if(!strncmp(&row->render[i],scs,scs_len)){				
				memset(&row->high_lighted[i],HL_COMMENT,*row->render_size - i); 
				break;
			}

		}
		if(mlcs_len && mlce_len && !in_string){
			if(in_comment){
				row->high_lighted[i] = HL_MLCOMMENT;
				if(!strncmp(&row->render[i],mlce,mlce_len)){
					memset(&row->high_lighted[i], HL_MLCOMMENT, mlce_len);
					i += mlce_len;
					in_comment = 0;
					prev_sep = 1;
					continue;
				}else{
					i++;
					continue;
				}

			}else if(!strncmp(&row->render[i],mlcs,mlcs_len)){
				memset(&row->high_lighted[i], HL_MLCOMMENT, mlcs_len);
				i += mlcs_len;
				in_comment = 1;
				continue;
			}
		}
		if(config->syntax->flags & HIGH_LIGHT_STRINGS){
			if(in_string){
				row->high_lighted[i] = HL_STRING;
				if(c == '\\' && (i + 1) < *row->render_size){
					row->high_lighted[i + 1] = HL_STRING;
					i+=2;
					continue;

				}
				if(c == in_string){
					in_string = 0;
				}
				i++;
				prev_sep = 1;
				continue;	
			}else{
				if(c == '"' || c == '\''){
					in_string = c;
					row->high_lighted[i] = HL_STRING;
					i++;
					continue;
				}
			}
		}
	
		if(config->syntax->flags & HIGH_LIGHT_NUMBERS){
	 		if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || 
			   ( c == '.' && prev_hl == HL_NUMBER)){	//TODO possible bug with a sentence that ends with a number.
				row->high_lighted[i] = HL_NUMBER;
				i++;
				prev_sep = 0;
				continue;
			}	
		}
	
		if(prev_sep){
			int j = 0;
			for(j = 0 ;keywords[j]; j++){
				int klen = strlen(keywords[j]);
				int kw2 = keywords[j][klen - 1] == '|';
				if(kw2){
					klen--;
				}
				if(!strncmp(&row->render[i],keywords[j],klen) && Is_Seperator(row->render[i + klen])){
					memset(&row->high_lighted[i], kw2 ? HL_KEYWORD_2 : HL_KEYWORD_1, klen);
					i += klen;
					break;
				}
			}
			if(keywords[j] != NULL){
				prev_sep = 0;
				continue;
			}
		}
		prev_sep = Is_Seperator(c);
		i++;	
	}
	int changed = (*row->hl_open_comment != in_comment);
	*row->hl_open_comment = in_comment;
	if(changed && (*row->idx + 1 )< *config->num_of_rows){
		Update_Syntax(&config->row[*row->idx + 1]);
	}
}

int Syntax_Color( int highlight )
{
	switch(highlight){
		case HL_COMMENT:
		case HL_MLCOMMENT:
			return 36;
		case HL_KEYWORD_1:
			return 33;
		case HL_KEYWORD_2:
			return 32;
		case HL_STRING:
			return 35;	
		case HL_NUMBER:
			return 31;
		case HL_MATCH:
			return 34;
		default:
			return 37;
	}
}

void Select_Syntax_High_Light()
{
	config->syntax = NULL;
	if(config->filename == NULL){
		return;
	}
	
	char *ext = strrchr(config->filename, '.');
	unsigned int j = 0;
	
	for(; j < HLDB_ENTRIES; j++){
		struct Syntax *s = &HLDB[j];
		unsigned int i = 0;
		while(s->file_match[i]){
			int is_ext = (s->file_match[i][0] == '.');
			if((is_ext && ext && !strcmp(ext, s->file_match[i])) ||
			  (!is_ext  && strstr(config->filename, s->file_match[i]))){
				config->syntax = s;

				int file_row = 0;
				for(file_row = 0; file_row < *config->num_of_rows; file_row++){
					Update_Syntax(&config->row[file_row]);
				}
				return;
			}
			i++;
		}
	}
}

/* ROW OPERATIONS */
int Row_Cursor_2_Render( File_row *row, int cx )
{
	int rx = 0, j =0;
	
	for( j = 0; j < cx; j++ ){
		if(row->string[j] == '\t'){
			rx += (TAB_STOP - 1) - (rx % TAB_STOP);
		}
		rx++;
	}
	return rx;
}

int Row_Rx_2_Cx( File_row *row, int rx )
{
	int cur_rx = 0;
	int cx = 0;
	for( cx = 0; cx < *row->size; cx++ ){
		if(row->string[cx] == '\t'){
			cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
		}
		cur_rx++;
		if(cur_rx > rx){
			return cx;
		}
	}

	return cx;
}

void Update_Row( File_row *row )
{
	int j = 0, idx = 0, tabs = 0;

	for( j = 0; j < *row->size; j++){
		if(row->string[j] == '\t'){
			tabs++;
		}
	}

	free( row->render );						
	row->render = malloc( *row->size + (tabs * (TAB_STOP - 1)) + 1 );
	
	for( j = 0; j < *row->size; j++ ){
		if(row->string[j] == '\t'){
			row->render[idx++] = ' ';
			while( ( idx % TAB_STOP )!= 0){
				row->render[idx++] = ' ';
			}
		}else{
			row->render[idx++] = row->string[j];
		}
	}
	row->render[idx] = '\0';
	*row->render_size = idx;
	Update_Syntax(row);
}

void Insert_Row( int index, char *line, size_t linelen )
{			
	if(index < 0 || index > *config->num_of_rows){
		return;
	}
	
	config->row = realloc(config->row, sizeof(File_row) * (*config->num_of_rows + 1));
	Check_Mem(config->row,"config->row");

	memmove(&config->row[index + 1], &config->row[index], sizeof(File_row) * (*config->num_of_rows - index));
	config->row[index].idx = malloc(sizeof(int));
	for(int j = index + 1; j <= *config->num_of_rows; j++ ){
		(*config->row[j].idx)++; 
	}

	*config->row[index].idx = index;	

	config->row[index].size = malloc(sizeof(int));
	*config->row[index].size = linelen;

	config->row[index].string = malloc((linelen + 1));
	memcpy(config->row[index].string, line, linelen);
	config->row[index].string[linelen] = '\0';

	config->row[index].render_size = malloc(sizeof(int));
	*config->row[index].render_size = 0;

	config->row[index].hl_open_comment = malloc(sizeof(int));
	*config->row[index].hl_open_comment = 0;	
	
	config->row[index].render = NULL;
	config->row[index].high_lighted = NULL;

	Update_Row( &config->row[index] );
	(*config->num_of_rows)++;
	(*config->dirty_flag)++;
}

void Row_Free( File_row *row )
{
	free_mem(row->idx,"row->idx");
	free_mem(row->hl_open_comment,"row->hl_open_comment");
	free_mem(row->high_lighted,"row->high_lighted");
	free_mem(row->render,"row->render");
	free_mem(row->render_size,"row->render_size");
	free_mem(row->string,"row->string");
	free_mem(row->size,"row->size");
}

void Del_Whole_Row( int row_num )
{
	if(row_num < 0 || row_num >= *config->num_of_rows){
		return;
	}
	Row_Free(&config->row[row_num]);
	memmove(&config->row[row_num], &config->row[row_num + 1], 
          sizeof(File_row) * (*config->num_of_rows - row_num - 1));
          
	for(int j = row_num; j < *config->num_of_rows - 1; j++){
		(*config->row[j].idx)--;
	}
	(*config->num_of_rows)--;
	(*config->dirty_flag)++;
}

void Row_Insert_Char( File_row *row, int x, int input )
{	
	row->string = realloc(row->string, *row->size + 2);
	if(x < 0 || x > *row->size){
		x = *row->size;	
	}

	memmove(&row->string[x + 1], &row->string[x], *row->size - x + 1);
	(*row->size)++;
	row->string[x] = input;
	Update_Row(row);
	(*config->dirty_flag)++;
}

void Row_Append_String( File_row *row, char *string, size_t len	)
{
	row->string = realloc(row->string, *row->size + len + 1);
	memcpy(&row->string[*row->size],string, len);
	*row->size += len;
	row->string[*row->size] = '\0';
	Update_Row(row);
	(*config->dirty_flag)++;
}

void Row_Delete_Char( File_row *row, int x )
{
	if(x < 0 || x >= *row->size){					
		return;	
	}
	memmove(&row->string[x],&row->string[x + 1], *row->size - x);
	(*row->size)--;
	Update_Row(row);
	(*config->dirty_flag)++;
}

/* EDITOR OPERATIONS */
void Editor_Insert_Char( int key_press )
{
	if(*config->cursor_y == *config->num_of_rows){
		Insert_Row(*config->num_of_rows, "",0);
	}
	Row_Insert_Char( &config->row[*config->cursor_y],*config->cursor_x, key_press);
	(*config->cursor_x)++;
}

void Editor_Insert_Newline()
{
	if(*config->cursor_x == 0){
		Insert_Row(*config->cursor_y, "", 0);
	}else{
		File_row *row = &config->row[*config->cursor_y];
		Insert_Row(*config->cursor_y + 1, &row->string[*config->cursor_x], 
                *row->size - *config->cursor_x);
		row = &config->row[*config->cursor_y];	
		*row->size = *config->cursor_x;
		row->string[*row->size] = '\0';
		Update_Row(row);
	}
	(*config->cursor_y)++;
	*config->cursor_x = 0;
}

void Editor_Delete_Char()
{
	if(*config->cursor_y == *config->num_of_rows){
		return;
	}

	if(*config->cursor_x == 0 && *config->cursor_y == 0){
		return;
	}
	File_row *row = &config->row[*config->cursor_y];
	if(*config->cursor_x > 0){
		Row_Delete_Char(row,*config->cursor_x - 1);
		(*config->cursor_x)--;
	}else{
		*config->cursor_x = *config->row[*config->cursor_y - 1].size;
		Row_Append_String(&config->row[*config->cursor_y - 1], row->string, *row->size);
		Del_Whole_Row(*config->cursor_y);
		(*config->cursor_y)--;	
	}
}
							
/* FILE INPUT/OUTPUT */
char *Rows_To_String( int *buff_len )
{
	int total_len = 0, i = 0;
	for(i = 0; i < *config->num_of_rows; i++){	
		total_len += *config->row[i].size + 1;
		*buff_len = total_len;
	}

	char *buff = malloc(total_len);
	char *p	 = buff;
	for(i = 0; i < *config->num_of_rows; i++){
		memcpy(p,config->row[i].string,*config->row[i].size);
		p += *config->row[i].size;
		*p = '\n';
		p++;
	}
	return buff;
}

void Open_File( char *filename )
{
	char *line = NULL;
	size_t fn_len = 0;
	size_t linecap = 0;
	ssize_t linelen = 0;

	FILE *fp = fopen(filename,"r+");
	if(!fp){
		fp = fopen(filename,"w+");
		if(!fp){
			perror("fopen: ");
			exit(1);		
		}
	}
	fn_len = strlen(filename);
	config->filename = strndup(filename, fn_len + 1);

	Select_Syntax_High_Light();
	
	while((linelen = getline(&line,&linecap,fp)) != -1){
		if(linelen != -1){
			while(linelen > 0 && (line[linelen - 1 ] == '\n' || line[linelen - 1] == '\r')){
				linelen--;
			}
			Insert_Row(*config->num_of_rows, line, linelen);
		} 
	}
	free(line);
	fclose(fp);
	*config->dirty_flag = 0;
}

void Save_File()
{
	if( config->filename == NULL){
		config->filename = Prompt("Save as: %s",NULL);
		if(!config->filename){
			Set_Status_Message("Save Cancelled.");
			return;
		}
		
		Select_Syntax_High_Light();
	}
	int len = 0;
	char *buff = Rows_To_String(&len);

	int fd = open(config->filename, O_RDWR | O_CREAT, 0644);
	if(fd != -1){						//ERROR HANDLING.
		if(ftruncate(fd,len) != -1){
			if(write(fd, buff, len) == len){
				close(fd);
				free(buff);
				Set_Status_Message("%s Filename Saved, %d Bytes Written.",config->filename,len);
				*config->dirty_flag = 0;
				return;
			}
		}
		close(fd);
	}
	Set_Status_Message("Error Saving: %s",strerror(errno));
	free(buff);
}

/* SEARCH */
void Find_Call_Back( char *query, int key_press )
{
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line;
	static char *saved_hl = NULL;

	if(saved_hl){
		memcpy(config->row[saved_hl_line].high_lighted, saved_hl, 
           *config->row[saved_hl_line].render_size);
		free_mem(saved_hl,"saved_hl");
		saved_hl = NULL;
	}

	if(key_press == '\r' || key_press == '\x1b'){
		last_match = -1;
		direction = 1;
		return;
	}else if(key_press == ARROW_RIGHT || key_press == ARROW_DOWN){
		direction = 1;
	}else if(key_press == ARROW_LEFT || key_press == ARROW_UP){
		direction = -1;
	}else{
		last_match = -1;
		direction = 1;
	}
	
	if(last_match == -1){
		direction = 1;
	}

	int current = last_match;
	int i = 0;
	for( i = 0; i < *config->num_of_rows; i++){
		current += direction;
		if(current == -1){
			current = *config->num_of_rows -1;
		}else if(current == *config->num_of_rows){
			current = 0;
		}
		File_row *row = &config->row[current];
		char *match = strstr(row->render,query);
		if(match){
			last_match = current;
			*config->cursor_y = current;
			*config->cursor_x = Row_Rx_2_Cx(row, match - row->render);
			*config->current_row = *config->num_of_rows;

			saved_hl_line = current;
			saved_hl = malloc(*row->render_size);
			memcpy(saved_hl, row->high_lighted, *row->render_size);
			
			memset(&row->high_lighted[match - row->render], HL_MATCH, strlen(query));
			break;
		}

	}
}

void Find()
{
	int saved_cursor_x = *config->cursor_x;
	int saved_cursor_y = *config->cursor_y;
	int saved_current_col = *config->current_col;
	int saved_current_row = *config->current_row;

	char *query = Prompt("Search %s (USE ESC/ARROWS/ENTER)",Find_Call_Back);
	if(query){
		free_mem(query,"query");
	}else{
		*config->cursor_x = saved_cursor_x;
		*config->cursor_y = saved_cursor_y;
		*config->current_col = saved_current_col;
		*config->current_row = saved_current_row;
	}
}

/* APPEND BUFFER */
void Append_Buffer( struct Buffer *buff, const char *key_press, int size )
{	
	char *new_buff = realloc(buff->string, buff->length + size);
	int length = buff->length;
	if( new_buff == NULL){
		return;							
	}
	memcpy(&new_buff[length],key_press,size);
	buff->string = new_buff;
	buff->length += size;
}

void Free_Buffer( struct Buffer *buff )
{
	if(buff->string){
		free_mem(buff->string,"buff->string");
	}
}

/* INPUT */
char *Prompt( char *prompt, void( *callback )( char *, int ) )
{
	size_t buff_size = 128;
	char *buff = malloc(sizeof(buff_size));
	
	size_t buff_len = 0;
	buff[0] = '\0';
	
	while(1){
		Set_Status_Message(prompt,buff);
		Refresh_Screen();
		
		int key_press = Read_Key();
		if(key_press == DEL_KEY || key_press == BACK_SPACE || key_press == CTRL_KEY('h')){
			if(buff_len != 0){
				buff[--buff_len] = '\0';
			}		
		}else if(key_press == '\x1b'){
			Set_Status_Message("");
			if(callback){
				callback(buff, key_press);
			}
			free_mem(buff,"buff");
			return NULL;
		}
		if(key_press == '\r'){
			if(buff_len != 0){
				Set_Status_Message("");
				if(callback){
					callback(buff,key_press);
				}
				return buff;
			}
		}else if(!iscntrl(key_press) && key_press < 128){
			if(buff_len == buff_size - 1){
				buff_size *= 2;
				buff = realloc(buff,buff_size);
			}
			buff[buff_len++] = key_press;
			buff[buff_len] = '\0';
		}
		if(callback){
			callback(buff,key_press);	
		}
	}
}

void Move_Cursor( int key_press )
{
	File_row *mc_row = (*config->cursor_y >= *config->num_of_rows ? NULL : &config->row[*config->cursor_y]);
	switch(key_press){
	case ARROW_UP:
		if(*config->cursor_y != 0){
			(*config->cursor_y)--;
		}
		break;
	case ARROW_LEFT:
		if(*config->cursor_x != 0){
			(*config->cursor_x)--;
		}else if(*config->cursor_y > 0){
			(*config->cursor_y)--;
			*config->cursor_x = *config->row[*config->cursor_y].size;
		}
		break;
	case ARROW_DOWN:
		if(*config->cursor_y < *config->num_of_rows){
			(*config->cursor_y)++;
		}	
		break;
	case ARROW_RIGHT:
		if(mc_row && *config->cursor_x < *mc_row->size){
			(*config->cursor_x)++;
		}else if(mc_row && *config->cursor_x == *mc_row->size){
			(*config->cursor_y)++;
			*config->cursor_x = 0;
		}
		break;
	default:
		printf("UNKNOWN INPUT\r\n");
		break;
	}
	mc_row = (*config->cursor_y >= *config->num_of_rows) ? NULL : &config->row[*config->cursor_y];
	int row_len = mc_row ? *mc_row->size : 0;
	if(*config->cursor_x > row_len){
		*config->cursor_x = row_len;	
	}
}

void Process_Key_Press()
{
	static int quit_times = TEDIT_QUIT;
	int times = 0; 
	int key_press = Read_Key();
	switch(key_press){
		case '\r':
			Editor_Insert_Newline();
			break; 

		case CTRL_KEY('s'):
			Save_File();
			break;	

		case CTRL_KEY('q'):
			if( *config->dirty_flag && quit_times > 0){
				Set_Status_Message("WARNING UNSAVED DATA WILL BE LOST. Press CTRL + Q %d More Times to Quit.",quit_times);
				quit_times--;
				return;
			}
			if(write(STDOUT,"\x1b[2J",4) == -1){
				die("switch[q]");
			}
			if(write(STDOUT,"\x1b[H",3) == -1){
				die("Refresh_Screen(Top)");
			}
			exit(0);

		case HOME_KEY:
			*config->cursor_x = 0;
			break;

		case END_KEY:
			if(*config->cursor_y < *config->num_of_rows){
				*config->cursor_x = *config->row[*config->cursor_y].size;
			}
			break;

		case CTRL_KEY('f'):
			Find();
			break;

		case BACK_SPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			if(key_press == DEL_KEY ){
				if(*config->cursor_y == *config->num_of_rows - 1 && *config->cursor_x >= *config->row[*config->cursor_y].size){
					return;
				}
				Set_Status_Message("num: %d",*config->num_of_rows);
				Move_Cursor(ARROW_RIGHT);
			}
			Editor_Delete_Char();
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			if(key_press == PAGE_UP){
				*config->cursor_y = *config->current_row;
			}else if( key_press == PAGE_DOWN){
				*config->cursor_y = *config->current_row + *config->screen_rows - 1;
			}

			if(*config->cursor_y > *config->num_of_rows){
			*config->cursor_y = *config->num_of_rows;		
			}

			times = *config->screen_rows;
			while(times--){
				Move_Cursor(key_press == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;

		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_DOWN:
		case ARROW_RIGHT:
			Move_Cursor(key_press);
			break;

		case CTRL_KEY('l'):
		case '\x1b':
			break;
		
	default:
		Editor_Insert_Char(key_press);
		break;
	}
	quit_times = TEDIT_QUIT;
}

/* OUTPUT */
void Set_Status_Message( const char *fmt, ... )
{
	va_list list;
	va_start(list,fmt);
	
	vsnprintf(config->status_msg ,sizeof( char ) * 80, fmt, list);
	va_end(list);
	config->status_time = time(NULL);
}

void Draw_Status_Bar( struct Buffer *buff )
{
	Append_Buffer(buff,"\x1b[7m",4);	// 7m for inverted colors

	char status_bar[80], render_bar[80];
	int len = snprintf(status_bar,sizeof(status_bar),"%.20s - %d lines %s", 
					   config->filename ? config->filename : "[No Name]", 
					   *config->num_of_rows, *config->dirty_flag ? "(Modified)": "");
	int rlen = snprintf(render_bar,sizeof(render_bar), "%s | %d%d",
						config->syntax ?  config->syntax->file_type : "no ft", 
						*config->cursor_y,*config->num_of_rows);
	if(len > *config->screen_cols){
		len = *config->screen_cols;	
	}
	Append_Buffer(buff,status_bar,len);
	for(; len < *config->screen_cols; len++){
		if(*config->screen_cols - len == rlen){
			Append_Buffer(buff,render_bar,rlen);
			break;
		}else{
			Append_Buffer(buff," ", 1);
		}
	}
	Append_Buffer(buff,"\x1b[m",3);	
	Append_Buffer(buff,"\r\n",2);
}

void Draw_Message_Bar( struct Buffer *buff )
{
	Append_Buffer(buff,"\x1b[K",3);
	int msg_len = strlen(config->status_msg);
	if( msg_len > *config->screen_cols){
		msg_len  = *config->screen_cols;
	}
	if( msg_len && time(NULL) - config->status_time < 5){
		Append_Buffer(buff, config->status_msg, msg_len);
	}
}

void Scroll()
{
	*config->render_x = *config->cursor_x;
	if(*config->cursor_y < *config->num_of_rows){
		*config->render_x = Row_Cursor_2_Render( &config->row[*config->cursor_y],*config->cursor_x);
	}
	if(*config->cursor_y < *config->current_row){
		*config->current_row = *config->cursor_y;		
	}
	if(*config->cursor_y >= *config->current_row + *config->screen_rows){
		*config->current_row = *config->cursor_y - *config->screen_rows + 1;	
	}
	if(*config->render_x < *config->current_col){
		*config->current_col = *config->render_x;
	}
	if(*config->render_x >= *config->current_col + *config->screen_cols){
		*config->current_col = (*config->render_x - *config->screen_cols) + 1;
	}
}
	
void Draw_Rows( struct Buffer *buff )
{
	int y = 0;
	for( y = 0 ; y < *config->screen_rows ; y++){
		int file_row = y + *config->current_row;
		if( file_row >= *config->num_of_rows){
			if( *config->num_of_rows == 0 && y == *config->screen_rows / 3){
				char welcome[64];		//welcome buffer
				int welcome_len = snprintf(welcome,sizeof(welcome),"Tedit-or Version %s",TEDIT_VERSION);
				welcome[welcome_len] = '\0';
				if(welcome_len > *config->screen_cols){
					welcome_len = *config->screen_cols;
				}
				int padding = (*config->screen_cols - welcome_len) / 2;
				if(padding){
					Append_Buffer(buff,"~",1);
					padding--;
				}
				while(padding != 0){
					Append_Buffer(buff," ",1);
					padding--;
				}
				Append_Buffer(buff,welcome,welcome_len);
			}else{
				Append_Buffer(buff,"~",1);
			}
		}else{
			int len = *config->row[file_row].render_size - *config->current_col;
			if(len < 0){
				len = 0;
			}
			if(len > *config->screen_cols){
				len = *config->screen_cols;
			}
			char *c = &config->row[file_row].render[*config->current_col];
			unsigned char *highlight = &config->row[file_row].high_lighted[*config->current_col];
			int current_color = -1;

			int i = 0;
			for(i = 0; i < len ; i++){
				if(iscntrl(c[i])){
					char sym = (c[i] <= 26) ? '@' + c[i] : '?';
					Append_Buffer(buff,"\x1b[7m",4);
					Append_Buffer(buff,&sym,1);
					Append_Buffer(buff,"\x1b[m",3);
					if(current_color != -1){
						char b_buf[16];
						int clen = snprintf(b_buf,sizeof(b_buf),"\x1b[%dm", current_color);
						Append_Buffer(buff,b_buf,clen);
					}	
				}else if(highlight[i] == HL_NORMAL){	
					if(current_color != -1){
						Append_Buffer(buff,"\x1b[39m",5);
						current_color = -1;
					}
					Append_Buffer(buff,&c[i],1);
				}else{
					int color = Syntax_Color( highlight[i] );
					if(color != current_color){
						current_color = color;
						char c_buf[16];
						int c_len = snprintf(c_buf,sizeof(c_buf),"\x1b[%dm",color);
						Append_Buffer(buff,c_buf,c_len);
					}
					Append_Buffer(buff,&c[i],1);
				}
			}
			Append_Buffer(buff,"\x1b[39m",5);
		}
		Append_Buffer(buff,"\x1b[K",3);
		Append_Buffer(buff,"\r\n",2);
	}
}

void Refresh_Screen()
{
	Scroll();

	struct Buffer buff = BUFFER_CONSTR;

	Append_Buffer(&buff,"\x1b[?25l",6);
	Append_Buffer(&buff,"\x1b[H",3);

	Draw_Rows(&buff);
	Draw_Status_Bar(&buff);
	Draw_Message_Bar(&buff);

	char curs_buff[32];
	snprintf(curs_buff,sizeof(curs_buff),"\x1b[%d;%dH", 
			 (*config->cursor_y - *config->current_row) + 1, (*config->render_x - *config->current_col )+ 1 );
	Append_Buffer(&buff,curs_buff,strlen(curs_buff));
	
	Append_Buffer(&buff,"\x1b[?25h",6);
	
	write(STDOUT,buff.string,buff.length);
	Free_Buffer(&buff);
}

/* INIT */
void Init_Editor()
{
	*config->cursor_x = 0;	
	*config->cursor_y = 0;
	*config->num_of_rows = 0;
	*config->current_row = 0;
	*config->current_col = 0;
	*config->render_x = 0;
	*config->dirty_flag = 0;
	config->status_msg[0] = '\0';
	config->status_time = 0;
	config->row = NULL;
	config->filename = NULL;
	config->syntax = NULL;

	if(Get_Win_Size(config->screen_cols,config->screen_rows) == -1){
		die("Get_Win_size");
	}
	*config->screen_rows -= 2;
}

int main( int argc, char **argv )
{
	
	Enable_Raw_Mode();
	Init_Editor();
	if(argc >= 2){
		Open_File(argv[1]);
	}
	char key_press = '\0';
	
	Set_Status_Message("CTRL + Q = Quit || CTRL + S = Save || CTRL + f = Find");	

	while(1){
		Refresh_Screen();
		Process_Key_Press();
	}
	
	return 0;
}

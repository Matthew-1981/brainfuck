#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define INC '+'
#define DEC '-'
#define LSH '<'
#define RSH '>'
#define PRT '.'
#define INP ','
#define LBR '['
#define RBR ']'
#define FBR '?'

#define BRACE_PTR_SIZE 4

#define MACHINE_MEM_TYPE unsigned short
#define MACHINE_MEM_BITFIELD 16
#define MACHINE_MEM_SIZE (1ULL << MACHINE_MEM_BITFIELD)

#define InSymbolLst(x) (x==INC || x==DEC || x==LSH || x==RSH || x==PRT || x==INP || x==LBR || x==RBR)


// Stack

struct uLongNode {
    unsigned long value;
    struct uLongNode* next;
};

typedef struct uLongNode* uLongStack_t;

#define NEW_STACK NULL

void StackPush (uLongStack_t* stack, unsigned long value)
{
    struct uLongNode* new = malloc( sizeof(struct uLongNode) );
    new->value = value;
    new->next = *stack;
    *stack = new;
}

unsigned long StackPop (uLongStack_t* stack)
{
    if ( *stack == NULL )  return 0;  // error

    unsigned long out = (*stack)->value;
    struct uLongNode* toDelete = (*stack);
    *stack = (*stack)->next;

    free( toDelete );
    return out;
}

int StackEmpty (uLongStack_t* stack)
{
    return (*stack) == NULL;
}


// Compiler

int DumpInfo (FILE* compiledFile, int symbol, unsigned long number, unsigned short sizeofNumber)
{
    if ( fputc(symbol, compiledFile) == EOF )  return -1;

    const static unsigned long field = (1 << 8) - 1;
    int tmp;

    for ( long i = sizeofNumber - 1; i >= 0; i-- ) {
        tmp = ( number >> (8 * i) ) & field;
        if ( fputc(tmp, compiledFile) == EOF )  return -1;
    }

    return 0;
}

unsigned long CountSymbol (FILE* sourceFile, int symbol)
{
    int current;
    unsigned long count = 1;

    while (1) {
        current = fgetc(sourceFile);
        if ( current == EOF )         break;
        if ( !InSymbolLst(current) )  continue;
        if ( current != symbol )      break;

        count++;
    }

    if ( current != EOF )
        fseek(sourceFile, -1, SEEK_CUR);

    return count;
}

int Compile (FILE* sourceFile, FILE* compiledFile)
{
    int current;
    uLongStack_t braceStack = NEW_STACK;

    unsigned long count;
    size_t bitCount = 0;

    while (1) {
        current = fgetc( sourceFile );

        if ( current < 0 )            break;
        if ( !InSymbolLst(current) )  continue;

        switch ( (char)current ) {

            case INC:
                count = CountSymbol( sourceFile, INC );
                count = (unsigned int) count % 256;
                if ( DumpInfo( compiledFile, INC, count, 1 ) == -1 )  goto error;
                bitCount += 2;
                break;

            case DEC:
                count = CountSymbol( sourceFile, DEC );
                count = (unsigned int) count % 256;
                if ( DumpInfo( compiledFile, DEC, count, 1 ) == -1 )  goto error;
                bitCount += 2;
                break;

            case LSH:
                count = CountSymbol( sourceFile, LSH );
                count = (unsigned int) count % MACHINE_MEM_SIZE;
                if ( DumpInfo( compiledFile, LSH, count, sizeof(MACHINE_MEM_TYPE) ) == -1 )  goto error;
                bitCount += 1 + sizeof(MACHINE_MEM_TYPE);
                break;

            case RSH:
                count = CountSymbol( sourceFile, RSH );
                count = (unsigned int) count % MACHINE_MEM_SIZE;
                if ( DumpInfo( compiledFile, RSH, count, sizeof(MACHINE_MEM_TYPE) ) == -1 )  goto error;
                bitCount += 1 + sizeof(MACHINE_MEM_TYPE);
                break;

            case PRT:
                if ( DumpInfo( compiledFile, PRT, 0, 0 ) == -1 )  goto error;
                bitCount += 1;
                break;

            case INP:
                if ( DumpInfo( compiledFile, INP, 0, 0 ) == -1 )  goto error;
                bitCount += 1;
                break;

            case LBR:
                StackPush( &braceStack, bitCount );
                if ( DumpInfo( compiledFile, FBR, 0, BRACE_PTR_SIZE ) == -1 )  goto error;
                bitCount += 1 + BRACE_PTR_SIZE;
                break;

            case RBR:
                if ( StackEmpty(&braceStack) )  goto error;
                count = StackPop( &braceStack );
                fseek( compiledFile, count, SEEK_SET );
                if ( DumpInfo( compiledFile, LBR, bitCount - count, BRACE_PTR_SIZE ) == -1 )  goto error;
                fseek( compiledFile, 0, SEEK_END );
                if ( DumpInfo( compiledFile, RBR, bitCount - count + BRACE_PTR_SIZE + 1, BRACE_PTR_SIZE ) == -1 )  goto error;
                bitCount += 1 + BRACE_PTR_SIZE;
                break;

        }
    }

    if ( !StackEmpty(&braceStack) )  goto error;

    return 0;

error:
    while ( !StackEmpty(&braceStack) )
        StackPop( &braceStack );
    return -1;
}


// Brainfuck machine

typedef struct {
    unsigned char memory[MACHINE_MEM_SIZE];
    MACHINE_MEM_TYPE ac : MACHINE_MEM_BITFIELD;
} brainfuck_t;

brainfuck_t* BrainfuckNew ()
{
    brainfuck_t* out = malloc( sizeof(brainfuck_t) );

    for ( size_t i = 0; i < MACHINE_MEM_SIZE; i++ )
        out->memory[i] = 0;

    out->ac = 0;
    return out;
}


// Interpreter

unsigned long GetNumber (unsigned char* bitCode, size_t bitCodeSize, size_t* pc, int size)
{
    unsigned long out = 0;

    for ( long i = size - 1; i >= 0; i-- ) {
        if ( *pc >= bitCodeSize )  break;
        int tmp = (int) bitCode[ (*pc)++ ];
        out += tmp << (8 * i);
    }

    return out;
}

int BrainfuckInterpret (brainfuck_t* machine, unsigned char* bitCode, size_t codeSize)
{
    char current;
    unsigned long count;
    size_t pc = 0;

    while ( pc < codeSize ) {
        current = bitCode[ pc++ ];

        switch ( current ) {

            case INC:
                count = GetNumber( bitCode, codeSize, &pc, 1 );
                machine->memory[ machine->ac ] += count;
                break;

            case DEC:
                count = GetNumber( bitCode, codeSize, &pc, 1 );
                machine->memory[ machine->ac ] -= count;
                break;

            case LSH:
                count = GetNumber( bitCode, codeSize, &pc, sizeof(MACHINE_MEM_TYPE) );
                machine->ac -= count;
                break;

            case RSH:
                count = GetNumber( bitCode, codeSize, &pc, sizeof(MACHINE_MEM_TYPE) );
                machine->ac += count;
                break;

            case PRT:
                fputc( machine->memory[ machine->ac ], stdout );
                break;

            case INP:
                count = fgetc( stdin );
                if ( count == EOF )
                    count = 0;
                machine->memory[ machine->ac ] = (unsigned char) count;
                break;

            case LBR:
                count = GetNumber( bitCode, codeSize, &pc, BRACE_PTR_SIZE );
                if ( machine->memory[ machine->ac ] == 0 )
                    pc += count;
                break;

            case RBR:
                count = GetNumber( bitCode, codeSize, &pc, BRACE_PTR_SIZE );
                pc -= count;
                break;

            default:
                return -1;
        }
    }

    return 0;
}


// Source code reader

long LoadFile (FILE* bitFile, unsigned char** ptr)
{
    fseek( bitFile, 0, SEEK_END );
    size_t fileSize = ftell( bitFile );
    fseek( bitFile, 0, SEEK_SET );

    *ptr = malloc( fileSize );

    if ( *ptr == NULL )  return -1;

    for ( size_t i = 0; i < fileSize; i++ ) {
        int current = fgetc( bitFile );
        (*ptr)[i] = (char) current;
    }

    return fileSize;
}

#define FILE_NAME_BUFFOR 1000
#define TMP_FILE_PATH "/tmp/%s%u.tmpcf"

int main (int argc, char** argv)
{
    int compile = 0;
    int execute = 0;
    int temporaryOutputFile = 1;
    int explicitOutputFile = 0;
    int explicitInputFile = 0;

    char outputFileName[FILE_NAME_BUFFOR];
    char* inputFileName;

    for ( int i = 1; i < argc; i++ ) {

        if ( strcmp(argv[i], "-ce") == 0 ) {
            if ( compile || execute ) {
                fprintf( stderr, "%s: duplicate falg(s) ('-c' or '-e')\n", argv[0] );
                return 1;
            }

            compile = 1;
            execute = 1;
            temporaryOutputFile = 0;

        } else if ( strcmp(argv[i], "-c") == 0 ) {
            if ( compile ) {
                fprintf( stderr, "%s: duplicate flag '-c'\n", argv[0] );
                return 1;
            }

            compile = 1;
            temporaryOutputFile = 0;

        } else if ( strcmp(argv[i], "-e") == 0 ) {
            if ( execute ) {
                fprintf( stderr, "%s: duplicate flag '-e'\n", argv[0] );
                return 1;
            }

            execute = 1;

        } else if ( strcmp(argv[i], "-o") == 0 ) {
            if ( explicitOutputFile ) {
                fprintf( stderr, "%s: output file has already been specified\n", argv[0] );
                return 1;
            }
            if ( ++i >= argc ) {
                fprintf( stderr, "%s: output file has not been specified\n", argv[0] );
                return 1;
            }

            strlcpy( outputFileName, argv[i], FILE_NAME_BUFFOR );
            explicitOutputFile = 1;
            temporaryOutputFile = 0;

        } else {
            if ( explicitInputFile ) {
                fprintf( stderr, "%s: source file has already been specified\n", argv[0] );
                return 1;
            }

            explicitInputFile = 1;
            inputFileName = argv[i];
        }

    }

    if ( !explicitInputFile ) {
        fprintf( stderr, "%s: no input file\n", argv[0] );
        return 1;
    }

    if ( !compile && !execute ) {
        ssize_t i = strlen( inputFileName ) - 1;
        while ( i >= 0 && inputFileName[i] != '.' )  i--;

        if ( i == -1 || i == 0 ) {
            compile = 1;
            execute = 1;
        } else if ( strcmp(inputFileName + i, ".bf") == 0 ) {
            compile = 1;
            execute = 1;
        } else if ( strcmp(inputFileName + i, ".cf") == 0 ) {
            execute = 1;
        } else {
            fprintf( stderr, "%s: file extension unrecognised\n", argv[0] );
            return 1;
        }
    }

    if ( !compile && explicitOutputFile ) {
        fprintf( stderr, "%s: explicit output file specified despite compilation is not going to happen\n", argv[0] );
        return 1;
    }

    if ( temporaryOutputFile ) {
        sprintf( outputFileName, TMP_FILE_PATH, argv[0], getpid() );
    } else if ( !explicitOutputFile ) {
        sprintf( outputFileName, "./out.cf" );
    }

    if ( compile ) {
        FILE* sourceFile = fopen( inputFileName, "r" );
        if ( sourceFile == NULL )  goto fileError;
        FILE* compiledFile = fopen( outputFileName, "wb" );
        if ( compiledFile == NULL )  { fclose(sourceFile); goto fileError; }

        int out = Compile( sourceFile, compiledFile );

        fclose( sourceFile );
        fclose( compiledFile );

        if ( out != 0 ) {
            fprintf( stderr, "%s: compilation has been unsuccessfull\n", argv[0] );
            unlink( outputFileName );
            return 2;
        }

        strlcpy( inputFileName, outputFileName, FILE_NAME_BUFFOR );
    }

    if ( execute ) {
        FILE* bitFile = fopen( inputFileName, "rb" );
        if ( bitFile == NULL )  goto fileError;

        brainfuck_t* machine = BrainfuckNew();
        unsigned char* bitcode;

        long bitcodeSize = LoadFile( bitFile, &bitcode );
        fclose( bitFile );
        int out = BrainfuckInterpret( machine, bitcode, bitcodeSize );

        free( machine );
        free( bitcode );

        if ( out != 0 ) {
            fprintf( stderr, "%s: corrupted bit file\n", argv[0] );
            return 3;
        }
    }

    if ( temporaryOutputFile )
        unlink( outputFileName );

    return 0;

fileError:
    fprintf( stderr, "%s: unable to open given files\n", argv[0] );
    return 4;
}

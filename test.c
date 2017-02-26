
#include <memalign.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libretro.h"
#include "test.h"


#define nb_saved_states 10000
#define nb_players_max 10
#define nb_state_per_player 2

int statePlayerHistory[nb_players_max][nb_saved_states];
int differentHistoryFlag[nb_players_max] = { 0 };
struct State {
    int statePlayer[nb_players_max];
    int globalState;
    int frameNumber;
} State;

int stateKeyBoard[nb_players_max][nb_state_per_player] = { { 0 } };

struct State stateTest = {
    { 0 },
    0
};

#define WIDTH 320
#define HEIGHT 200
#define SAMPLE_RATE 48000.0f
#define FPS_RATE 60.0
#define log_error(...) log_cb(RETRO_LOG_ERROR,__VA_ARGS__);
#define log_info(...) log_cb(RETRO_LOG_INFO,__VA_ARGS__);

static uint32_t *frame_buf;
static struct retro_log_callback logging;
retro_log_printf_t log_cb;
retro_audio_sample_batch_t audio_batch_cb;
static char retro_save_directory[4096];
static char retro_base_directory[4096];
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

// joypads

#define DESC_NUM_PORTS(desc) ((desc)->port_max - (desc)->port_min + 1)
#define DESC_NUM_INDICES(desc) ((desc)->index_max - (desc)->index_min + 1)
#define DESC_NUM_IDS(desc) ((desc)->id_max - (desc)->id_min + 1)

#define DESC_OFFSET(desc, port, index, id) ( \
port * ((desc)->index_max - (desc)->index_min + 1) * ((desc)->id_max - (desc)->id_min + 1) + \
index * ((desc)->id_max - (desc)->id_min + 1) + \
id \
)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct descriptor {
    int device;
    int port_min;
    int port_max;
    int index_min;
    int index_max;
    int id_min;
    int id_max;
    uint16_t *value;
};

static struct descriptor joypad = {
    .device = RETRO_DEVICE_JOYPAD,
    .port_min = 0,
    .port_max = 7,
    .index_min = 0,
    .index_max = 0,
    .id_min = RETRO_DEVICE_ID_JOYPAD_B,
    .id_max = RETRO_DEVICE_ID_JOYPAD_R3
};

static struct descriptor *descriptors[] = {
    &joypad
};

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

void retro_init(void)
{
    log_cb(RETRO_LOG_DEBUG, "retro_init");

    struct descriptor *desc;
    int size;
    int i;

    const char *dir = NULL;
    sprintf(retro_base_directory,"/tmp");
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
    {
        if (strlen(dir)) {
            snprintf(retro_base_directory, sizeof(retro_base_directory), "%s", dir);
        }
    }

    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
    {
        // If save directory is defined use it, otherwise use system directory
        if (strlen(dir))
            snprintf(retro_save_directory, sizeof(retro_save_directory), "%s", dir);
        else
            snprintf(retro_save_directory, sizeof(retro_save_directory), "%s", retro_base_directory);
    }

    frame_buf = calloc(WIDTH * HEIGHT, sizeof(uint32_t));

    /* joypads Allocate descriptor values */
    for (i = 0; i < ARRAY_SIZE(descriptors); i++) {
        desc = descriptors[i];
        size = DESC_NUM_PORTS(desc) * DESC_NUM_INDICES(desc) * DESC_NUM_IDS(desc);
        descriptors[i]->value = (uint16_t*)calloc(size, sizeof(uint16_t));
    }
}

void retro_deinit(void)
{
    int i;
    free(frame_buf);
    frame_buf = NULL;
    /* Free descriptor values */
    for (i = 0; i < ARRAY_SIZE(descriptors); i++) {
        free(descriptors[i]->value);
        descriptors[i]->value = NULL;
    }
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    log_cb(RETRO_LOG_INFO, "%s: Plugging device %u into port %u.\n", __FILE__, device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name     = __FILE__;
    #ifdef GIT_VERSION
       info->library_version  = "1" GIT_VERSION;
    #else
       info->library_version  = "1";
    #endif
    info->need_fullpath    = false;
    info->valid_extensions = NULL;
}



void retro_get_system_av_info(struct retro_system_av_info *info)
{
    float aspect = 5.0f / 3.0f;

    float sampling_rate = SAMPLE_RATE;

    info->timing = (struct retro_system_timing) {
        .fps = FPS_RATE,
        .sample_rate = sampling_rate,
    };


    info->geometry = (struct retro_game_geometry) {
        .base_width   = WIDTH,
        .base_height  = HEIGHT,
        .max_width    = WIDTH,
        .max_height   = HEIGHT,
        .aspect_ratio = aspect,
    };

}

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    bool no_content = true;

    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = fallback_log;

}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_reset(void)
{
}



static void update_input(void)
{
    struct descriptor *desc;
    uint16_t state;
    int offset;
    int port;
    int index;
    int id;
    int i;

    /* Poll input */
    input_poll_cb();

    /* Parse descriptors */
    for (i = 0; i < ARRAY_SIZE(descriptors); i++) {
        /* Get current descriptor */
        desc = descriptors[i];

        /* Go through range of ports/indices/IDs */
        for (port = desc->port_min; port <= desc->port_max; port++)
        for (index = desc->index_min; index <= desc->index_max; index++)
        for (id = desc->id_min; id <= desc->id_max; id++) {
            /* Compute offset into array */
            offset = DESC_OFFSET(desc, port, index, id);

            /* Get new state */
            state = input_state_cb(port,
                                   desc->device,
                                   index,
                                   id);
            /* Update state */
            desc->value[offset] = state;
            test_update_input(id,port,state);
        }
    }
}

/*
void update_vga(uint32_t *buf, unsigned stride) {
    static uint32_t matrixPalette[NB_COLORS_PALETTE];
    int z=0;
    do {
        matrixPalette[z/3]= ((m.vgaPalette[z]*4) << 16) | ((m.vgaPalette[z+1]*4) << 8) | (m.vgaPalette[z+2]*4);
        z+=3;
    } while (z!=NB_COLORS_PALETTE*3);
    uint32_t *line = buf;
    for (unsigned y = 0; y < HEIGHT; y++, line += stride)
    {
        for (unsigned x = 0; x < WIDTH; x++)
        {
            if (y<HEIGHT) {
                line[x] = matrixPalette[m.vgaRam[x+y*WIDTH]];
            }
        }
    }
}
*/

static void render_checkered(void)
{
    /* Try rendering straight into VRAM if we can. */
    uint32_t *buf = NULL;
    unsigned stride = 0;
    struct retro_framebuffer fb = {0};
    fb.width = WIDTH;
    fb.height = HEIGHT;
    fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
    if (environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb) && fb.format == RETRO_PIXEL_FORMAT_XRGB8888)
    {
        buf = fb.data;
        stride = fb.pitch >> 2;
    }
    else
    {
        buf = frame_buf;
        stride = WIDTH;
    }
    //update_vga(buf,stride);
    video_cb(buf, WIDTH, HEIGHT, stride << 2);
}


static void check_variables(void)
{
}


void retro_run(void)
{
    stateTest.frameNumber++;
    update_input();
    render_checkered();
    compute_all_states();
    /*
    if (m.executionFinished) {
        log_cb(RETRO_LOG_INFO, "Exit.\n");
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    }
     */
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        check_variables();
}


bool retro_load_game(const struct retro_game_info *info)
{
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
        return false;
    }

    check_variables();

    (void)info;
    return true;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    return retro_load_game(NULL);
}

void *retro_get_memory_data(unsigned id)
{
    (void)id;
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    (void)id;
    return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

void show_message(char * message) {
    struct retro_message msg;
    msg.msg = message;
    msg.frames = 80;
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, (void*)&msg);
}



size_t retro_serialize_size(void)
{
    return sizeof(State);
}

bool retro_serialize(void *data_, size_t size)
{
    memcpy(data_, &stateTest, sizeof(State));
    return true;
}


bool retro_unserialize(const void *data_, size_t size)
{
    if (size!=sizeof(State)) {
        log_error("test_retro_unserialize error %d/%d\n",size,sizeof(State));
        return false;
    }
    struct State stateTest2;
    memcpy(&stateTest2,data_, sizeof(State));
    log_info("test_retro_unserialize: current frameNumber/state %d/%d rewind to: %d/%d\n",stateTest.frameNumber, stateTest.globalState,stateTest2.frameNumber, stateTest2.globalState);
    memcpy(&stateTest,data_, sizeof(State));
    return true;
}

int compute_state_player(int player) {
  int result=0;
  int i;
  for (i=0;i<nb_state_per_player;i++) {
    if (stateKeyBoard[player][i]) {
      result = result + ( 1 << ((player*nb_state_per_player)+i) );
    }
  }
  result++; // state will be > 0
  return result;
}

void compute_all_states() {
  int globalState=0;
  int i;
  int numberOfFlagSet=0;
  for (i=0;i<nb_players_max;i++) {
      int calculatedState=compute_state_player(i);
      stateTest.statePlayer[i]=calculatedState;
      
      if ((statePlayerHistory[i][stateTest.frameNumber%nb_saved_states]!=calculatedState) && (statePlayerHistory[i][stateTest.frameNumber%nb_saved_states])) {
          differentHistoryFlag[i]=1;
      }
      
      statePlayerHistory[i][stateTest.frameNumber%nb_saved_states]=calculatedState;
      globalState+=calculatedState;
      
      numberOfFlagSet+=differentHistoryFlag[i];
  }

    
    
  if(globalState!=stateTest.globalState) {
      log_info("test_calculte_new_state() frameNumber=%d new state:%d numberOfFlagSet=%d\n",stateTest.frameNumber,globalState,numberOfFlagSet);
      stateTest.globalState=globalState;
  }
}

void test_update_input(int keyid, int playerNumber,int state)
{
  int j=-1;
  switch (keyid)
  {
      case button_a:
          j=0;
      break;
      case button_b:
          j=1;
      break;
    default:
    break;
  }
  if(j!=-1) {
      if (stateKeyBoard[playerNumber][j]!=state) {
          log_info("test_update_input(keyid:%d playerNumber: %d state:%d) frameNumber=%d\n",keyid,playerNumber,state, stateTest.frameNumber);
      }
      stateKeyBoard[playerNumber][j]=state;
  }
}

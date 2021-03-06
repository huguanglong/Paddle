#include <paddle/capi.h>
#include <pthread.h>
#include <time.h>
#include "../common/common.h"

#define CONFIG_BIN "./trainer_config.bin"
#define NUM_THREAD 4
#define NUM_ITER 1000

pthread_mutex_t mutex;

void* thread_main(void* gm_ptr) {
  paddle_gradient_machine machine = (paddle_gradient_machine)(gm_ptr);
  paddle_arguments in_args = paddle_arguments_create_none();
  // Create input matrix.
  paddle_matrix mat = paddle_matrix_create(/* sample_num */ 1,
                                           /* size */ 784,
                                           /* useGPU */ false);
  paddle_arguments out_args = paddle_arguments_create_none();
  paddle_matrix prob = paddle_matrix_create_none();
  for (int iter = 0; iter < NUM_ITER; ++iter) {
    // There is only one input of this network.
    CHECK(paddle_arguments_resize(in_args, 1));

    paddle_real* array;

    // Get First row.
    CHECK(paddle_matrix_get_row(mat, 0, &array));

    for (int i = 0; i < 784; ++i) {
      array[i] = rand() / ((float)RAND_MAX);
    }

    CHECK(paddle_arguments_set_value(in_args, 0, mat));

    CHECK(paddle_gradient_machine_forward(machine,
                                          in_args,
                                          out_args,
                                          /* isTrain */ false));

    CHECK(paddle_arguments_get_value(out_args, 0, prob));

    CHECK(paddle_matrix_get_row(prob, 0, &array));

    pthread_mutex_lock(&mutex);
    printf("Prob: ");
    for (int i = 0; i < 10; ++i) {
      printf("%.2f ", array[i]);
    }
    printf("\n");
    pthread_mutex_unlock(&mutex);
  }

  CHECK(paddle_matrix_destroy(prob));
  CHECK(paddle_arguments_destroy(out_args));
  CHECK(paddle_matrix_destroy(mat));
  CHECK(paddle_arguments_destroy(in_args));
  CHECK(paddle_gradient_machine_destroy(machine));
  return NULL;
}

int main() {
  // Initalize Paddle
  char* argv[] = {"--use_gpu=False"};
  CHECK(paddle_init(1, (char**)argv));

  // Reading config binary file. It is generated by `convert_protobin.sh`
  long size;
  void* buf = read_config(CONFIG_BIN, &size);

  // Create a gradient machine for inference.
  paddle_gradient_machine machine;
  CHECK(paddle_gradient_machine_create_for_inference(&machine, buf, (int)size));
  CHECK(paddle_gradient_machine_randomize_param(machine));

  // Loading parameter. Uncomment the following line and change the directory.
  // CHECK(paddle_gradient_machine_load_parameter_from_disk(machine,
  //                                                "./some_where_to_params"));
  srand(time(0));
  pthread_mutex_init(&mutex, NULL);

  pthread_t threads[NUM_THREAD];

  for (int i = 0; i < NUM_THREAD; ++i) {
    paddle_gradient_machine thread_local_machine;
    CHECK(paddle_gradient_machine_create_shared_param(
        machine, buf, size, &thread_local_machine));
    pthread_create(&threads[i], NULL, thread_main, thread_local_machine);
  }

  for (int i = 0; i < NUM_THREAD; ++i) {
    pthread_join(threads[i], NULL);
  }

  pthread_mutex_destroy(&mutex);

  return 0;
}

#include "vmodem/vmcp.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    g_fail++;
  }
}

int main(void) {
  char line[128];
  vmcp_msg_t msg;
  size_t n;

  g_fail = 0;
  n = vmcp_format(VMCP_MSG_CALL, "200", 0, line, sizeof(line));
  expect(n > 0, "format CALL");
  expect(strstr(line, "VMCP/1 CALL 200") == line, "CALL text");

  expect(vmcp_parse(line, n, &msg) == VMOD_OK, "parse CALL");
  expect(msg.type == VMCP_MSG_CALL, "type CALL");
  expect(strcmp(msg.arg, "200") == 0, "arg 200");

  n = vmcp_format(VMCP_MSG_CONNECT, NULL, 9600, line, sizeof(line));
  expect(vmcp_parse(line, n, &msg) == VMOD_OK, "parse CONNECT");
  expect(msg.type == VMCP_MSG_CONNECT, "type CONNECT");
  expect(msg.speed == 9600, "speed 9600");

  n = vmcp_format(VMCP_MSG_ANSWER, NULL, 0, line, sizeof(line));
  expect(vmcp_parse(line, n, &msg) == VMOD_OK, "parse ANSWER");
  expect(msg.type == VMCP_MSG_ANSWER, "type ANSWER");

  if (g_fail) {
    fprintf(stderr, "%d vmcp tests failed\n", g_fail);
    return 1;
  }
  printf("test_vmcp: PASS\n");
  return 0;
}

#ifndef CC_PUBLISH_H
#define CC_PUBLISH_H

#define WORK "cc-work"
#define SAVE WORK "/previous"

int publish(const char *src, const char *dst, int *keep_backup);

#endif

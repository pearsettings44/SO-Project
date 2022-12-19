#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {

    char *str_ext_file =
        "kTdbEnWD0PQyssY52L9GCuWC00fLeqZoHDbll73ZoDABe18Jc4ybpu7yh1G8iuDjfsdpMlezr7Gdl5B4em3u2UmM8bUOq1QmhY78RIha7VK8pusfCeF6D9hXsOC34WO1ZNwTyfycR1MzQbspEWwL2ENw0smxaXDBt9Ry0YLFktiwDNgzrPwacwQT80F8pQBfgEOLrdPzYo7ycjaVIMlk66FLGPXg0aQPsrRztnwLzsiNhwQaf1BcGNcOrtsNdkqPtCG3feZzczeUu4yRXprlOiwLghDYwRz2xirA1XgvDQwOOMEVE24DmyyT1QRsbTLmtGCjUGOt5qcTJm2FXnCgq3IzzGjjQN0BanyaE2I8hgArmFk4nvLRnat6NvD3ANI4ryCf4vEOjiMH5WaqDQ5felzbANvfb88foqfqgu7SNjX74QNGOcplLJcREEWKQq3DNTIaHYJYdhfbDxZ4Uzok5LzAp2z3GX40jnUMb1h3PprueBIFDqrSOgJBYybX4bIVh9XufEKmXgRuRVVVcqKGhdWVzNk3HtKRggnyLHg96nw2LrPzk6iFtgIlMmIOCgxIQ3YRITRPZqC1qxfOkJJHQ38I9CTnLasKswFWsmlttNTZBoBJQoaCRr6FGGoiO2NxYIspII7GhCBSkOlzoTS4SqDblnhI07kFX8H3jmWuxjJSCri6D5qupWyecM20tyMt3Lvyp669pGJzmSdp8OpTND0WL3hwRQAD1betdAC63MNKrpL8eLi3vhqZSEVZsubH0RoYvgN2O95GV1YKQNpCoj2MRIvADOldw64SICkaKYI7BNhFCaLex0mf8EiRrqLkBBfqHkZvlfrO24vuZHXXkaaSgxbeEAHsUK0Kixip0OFblgz6m8As4r4xZNBuAIVXn45oKmkYaTb0DnzqJiYoW5ngZLEUv1o69xrirm3G43J7KtxqYWptM4G3u0i8a66fmI9egSVqt0MG8h00GqVzosUl9JtmiPpFhS4yeqB48YSEKeo9D5bvvEJo6qRQSGWf";
    char *path_copied_file = "/f1";
    char *path_src = "tests/file_to_copy_over1024.txt";
    char buffer[10000];

    assert(tfs_init(NULL) != -1);

    int f;
    ssize_t r;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    printf("Successful test.\n");

    return 0;
}

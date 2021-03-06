/**
 * @file megaraid.c
 * @author Copyright (c) 2012-2015 Astersmith, LLC
 * @author Marc A. Smith
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "prototypes.h"
#include "system.h"
#include "dialogs.h"
#include "megaraid.h"

/*
 * Get MegaCLI version -- just the actual number for now.
 */
char *getMegaCLIVersion() {
    FILE *megacli = NULL;
    char *command = NULL, *version_p = NULL;
    int status = 0;
    char line[MAX_MC_LINE] = {0}, version[20] = {0};

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -help -NoLog 2>&1", MEGACLI_BIN);
    megacli = popen(command, "r");

    /* Loop over command output */
    while (fgets(line, sizeof (line), megacli) != NULL) {
        if (strstr(line, "MegaCLI SAS RAID Management Tool  Ver")) {
            sscanf(line, "%*s %*s %*s %*s %*s %*s %s", version);
        }
    }

    status = pclose(megacli);
    FREE_NULL(command);
    if (status != 0) {
        return NULL;
    }

    /* Done */
    version_p = version;
    return version_p;
}


/*
 * Get number of adapters (RAID controllers).
 */
int getMRAdapterCount() {
    FILE *megacli = NULL;
    char *command = NULL, *mc_version = NULL;
    int count = 0;
    char line[MAX_MC_LINE] = {0};

    /* A cheezy check to see if MegaCLI is "working" -- see comments below */
    mc_version = getMegaCLIVersion();
    if (!mc_version) {
        return -1;
    }

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -adpCount -NoLog 2>&1", MEGACLI_BIN);
    megacli = popen(command, "r");

    /* Loop over command output */
    while (fgets(line, sizeof (line), megacli) != NULL) {
        if (strstr(line, "Controller Count:"))
            sscanf(line, "%*s %*s %d", &count);
    }

    /* For this one we ignore the exit code -- when using the 'adpCount' option
     * with MegaCLI, it also returns the adapter count as the exit code */
    pclose(megacli);

    /* Done */
    FREE_NULL(command);
    return count;
}


/*
 * Get adapter attributes from MegaCLI.
 */
MRADAPTER *getMRAdapter(int adapter_id) {
    MRADAPTER *adapter = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0;

    adapter = (MRADAPTER *) calloc(1, sizeof(MRADAPTER));
    if (adapter != NULL) {
        adapter->adapter_id = adapter_id;

        /* MegaCLI command */
        SAFE_ASPRINTF(&command, "%s -AdpAllInfo -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");

        /* Loop over command output */
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Product Name    :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->prod_name, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Serial No       :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->serial, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "FW Package Build:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->firmware, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "BBU              :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->bbu, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Memory Size      :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->memory, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Host Interface  :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->interface, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Virtual Drives    :")) {
                sscanf(line, "%*s %*s %*s %d", &adapter->logical_drv_cnt);

            } else if (strstr(line, "  Disks           :")) {
                sscanf(line, "%*s %*s %d", &adapter->disk_cnt);

            } else if (strstr(line, "Cluster Permitted     :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->cluster, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Cluster Active        :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(adapter->cluster_on, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);
            }
        }

        /* Done with MegaCLI */
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            FREE_NULL(adapter);
            return NULL;
        }
    }

    /* Done */
    return adapter;
}


/*
 * Get "settable" adapter properties from MegaCLI.
 */
MRADPPROPS *getMRAdapterProps(int adapter_id) {
    MRADPPROPS *adp_props = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0;

    adp_props = (MRADPPROPS *) calloc(1, sizeof(MRADPPROPS));
    if (adp_props != NULL) {
        adp_props->adapter_id = adapter_id;

        /* Get CacheFlushInterval */
        SAFE_ASPRINTF(&command, "%s -AdpGetProp CacheFlushInterval -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Cache Flush Interval")) {
                strtok_result = strtok(line, "=");
                strtok_result = strtok(NULL, "=");
                sscanf(strStrip(strtok_result), "%d", &adp_props->cache_flush);
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            return NULL;
        }

        /* Get RebuildRate */
        SAFE_ASPRINTF(&command, "%s -AdpGetProp RebuildRate -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Rebuild Rate")) {
                strtok_result = strtok(line, "=");
                strtok_result = strtok(NULL, "=");
                sscanf(strStrip(strtok_result), "%d", &adp_props->rebuild_rate);
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            return NULL;
        }

        /* Get ClusterEnable */
        SAFE_ASPRINTF(&command, "%s -AdpGetProp ClusterEnable -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Cluster :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strtok_result = strtok(NULL, ":");
                if (strcmp(strStrip(strtok_result), "Enabled") == 0) {
                    adp_props->cluster = TRUE;
                } else if (strcmp(strStrip(strtok_result), "Disabled") == 0) {
                    adp_props->cluster = FALSE;
                }
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            return NULL;
        }

        /* Get NCQDsply */
        SAFE_ASPRINTF(&command, "%s -AdpGetProp NCQDsply -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "NCQ Status is")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (strcmp(strStrip(strtok_result),
                        "NCQ Status is Enabled") == 0) {
                    adp_props->ncq = TRUE;
                } else if (strcmp(strStrip(strtok_result),
                        "NCQ Status is Disabled") == 0) {
                    adp_props->ncq = FALSE;
                }
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            FREE_NULL(adp_props);
            return NULL;
        }
    }

    /* Done */
    return adp_props;
}


/*
 * Set adapter properties via MegaCLI.
 */
int setMRAdapterProps(MRADPPROPS *adp_props) {
    char *command = NULL;
    int status = 0;

    /* Set CacheFlushInterval */
    SAFE_ASPRINTF(&command, "%s -AdpSetProp CacheFlushInterval -%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            adp_props->cache_flush, adp_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set RebuildRate */
    SAFE_ASPRINTF(&command, "%s -AdpSetProp RebuildRate -%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            adp_props->rebuild_rate, adp_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set ClusterEnable */
    SAFE_ASPRINTF(&command, "%s -AdpSetProp ClusterEnable -%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            (int) adp_props->cluster, adp_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set NCQEnbl/NCQDsbl */
    if (adp_props->ncq == TRUE)
        SAFE_ASPRINTF(&command, "%s -AdpSetProp NCQEnbl -a%d "
                "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
                adp_props->adapter_id);
    else
        SAFE_ASPRINTF(&command, "%s -AdpSetProp NCQDsbl -a%d "
                "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
                adp_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Done */
    return 0;
}


/*
 * Get MegaRAID disk information.
 */
MRDISK *getMRDisk(int adapter_id, int encl_id, int slot) {
    MRDISK *disk = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0;

    disk = (MRDISK *) calloc(1, sizeof(MRDISK));
    if (disk != NULL) {
        disk->adapter_id = adapter_id;
        disk->present = TRUE;
        disk->part_of_ld = FALSE;

        /* MegaCLI command */
        SAFE_ASPRINTF(&command, "%s -pdInfo -PhysDrv[%d:%d] -a%d -NoLog 2>&1",
                MEGACLI_BIN, encl_id, slot, adapter_id);
        megacli = popen(command, "r");

        /* Loop over command output */
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "is not found.")) {
                /* A disk isn't present in the specified slot */
                disk->present = FALSE;
                continue;

            } else if (strstr(line, "Enclosure Device ID:")) {
                sscanf(line, "%*s %*s %*s %d", &disk->enclosure_id);

            } else if (strstr(line, "Slot Number:")) {
                sscanf(line, "%*s %*s %d", &disk->slot_num);

            } else if (strstr(line, "PD Type:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(disk->pd_type, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Raw Size:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(disk->raw_size, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Firmware state:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(disk->state, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Inquiry Data:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(disk->inquiry, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Link Speed:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(disk->speed, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Drive's position:")) {
                /* If the PD entry contains the string above, then its
                 * part of a logical drive (LD). */
                disk->part_of_ld = TRUE;
            }
        }

        /* Done with MegaCLI */
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            FREE_NULL(disk);
            return NULL;
        }
    }

    /* Done */
    return disk;
}


/*
 * Get MegaRAID enclosure information; not happy about how this is
 * implemented -- if LSI could have just made all of the MegaCLI commands
 * uniform (eg, be able to specify an enclosure like you specify an adapter)!
 */
MRENCL *getMREnclosure(int adapter_id, int encl_id) {
    MRENCL *enclosure = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0;
    int counters[] = {0, 0, 0, 0, 0, 0, 0};

    enclosure = (MRENCL *) calloc(1, sizeof(MRENCL));
    if (enclosure != NULL) {
        enclosure->adapter_id = adapter_id;

        /* MegaCLI command */
        SAFE_ASPRINTF(&command, "%s -EncInfo -a%d -NoLog 2>&1",
                MEGACLI_BIN, adapter_id);
        megacli = popen(command, "r");

        /* Loop over command output */
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "    Device ID                     :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[0] == encl_id)
                    sscanf(strtok_result, " %d", &enclosure->device_id);
                counters[0]++;

            } else if (strstr(line, "    Number of Slots               :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[1] == encl_id)
                    sscanf(strtok_result, " %d", &enclosure->slots);
                counters[1]++;

            } else if (strstr(line, "    Number of Power Supplies      :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[2] == encl_id)
                    sscanf(strtok_result, " %d", &enclosure->power_supps);
                counters[2]++;

            } else if (strstr(line, "    Number of Fans                :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[3] == encl_id)
                    sscanf(strtok_result, " %d", &enclosure->fans);
                counters[3]++;

            } else if (strstr(line, "    Status                        :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[4] == encl_id)
                    strncpy(enclosure->status, strStrip(strtok_result),
                            MAX_MR_ATTR_SIZE);
                counters[4]++;

            } else if (strstr(line, "        Vendor Identification     :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[5] == encl_id)
                    strncpy(enclosure->vendor, strStrip(strtok_result),
                            MAX_MR_ATTR_SIZE);
                counters[5]++;

            } else if (strstr(line, "        Product Identification    :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                if (counters[6] == encl_id)
                    strncpy(enclosure->product, strStrip(strtok_result),
                            MAX_MR_ATTR_SIZE);
                counters[6]++;
            }
        }

        /* Done with MegaCLI */
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            FREE_NULL(enclosure);
            return NULL;
        }
    }

    /* Done */
    return enclosure;
}


/*
 * Get a count of the enclosures for the specified adapter.
 */
int getMREnclCount(int adapter_id) {
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    int status = 0, count = 0;
    char line[MAX_MC_LINE] = {0};

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -EncInfo -a%d -NoLog 2>&1", MEGACLI_BIN, adapter_id);
    megacli = popen(command, "r");

    /* Loop over command output */
    while (fgets(line, sizeof(line), megacli) != NULL) {
        if (strstr(line, "    Number of enclosures on adapter")) {
            strtok_result = strtok(line, "--");
            strtok_result = strtok(NULL, "--");
            sscanf(strtok_result, " %d", &count);
        }
    }

    /* Done with MegaCLI */
    status = pclose(megacli);
    FREE_NULL(command);
    if (status != 0) {
        return -1;
    }

    /* Done */
    return count;
}


/*
 * Get count of MegaRAID logical (virtual) drives for given adapter.
 */
int getMRLDCount(int adapter_id) {
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL, *mc_version = NULL;
    int count = 0;
    char line[MAX_MC_LINE] = {0};

    /* A cheezy check to see if MegaCLI is "working" -- see comments below */
    mc_version = getMegaCLIVersion();
    if (!mc_version) {
        return -1;
    }

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -LDGetNum -a%d -NoLog 2>&1",
            MEGACLI_BIN, adapter_id);
    megacli = popen(command, "r");

    /* Loop over command output */
    while (fgets(line, sizeof(line), megacli) != NULL) {
        if (strstr(line, "Number of Virtual Drives Configured on Adapter")) {
            strtok_result = strtok(line, ":");
            strtok_result = strtok(NULL, ":");
            sscanf(strtok_result, " %d", &count);
        }
    }

    /* For this one we ignore the exit code -- when using the 'LDGetNum' option
     * with MegaCLI, it also returns the LD count as the exit code */
    pclose(megacli);

    /* Done */
    FREE_NULL(command);
    return count;
}


/*
 * Get MegaRAID logical drive information.
 */
MRLDRIVE *getMRLogicalDrive(int adapter_id, int ldrive_id) {
    MRLDRIVE *logical_drive = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0;

    logical_drive = (MRLDRIVE *) calloc(1, sizeof(MRLDRIVE));
    if (logical_drive != NULL) {
        logical_drive->adapter_id = adapter_id;
        logical_drive->ldrive_id = ldrive_id;

        /* MegaCLI command */
        SAFE_ASPRINTF(&command, "%s -LDInfo -L%d -a%d -NoLog 2>&1",
                MEGACLI_BIN, ldrive_id, adapter_id);
        megacli = popen(command, "r");

        /* Loop over command output */
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "RAID Level          :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(logical_drive->raid_lvl, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Size                :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(logical_drive->size, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "State               :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(logical_drive->state, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Strip Size          :")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(logical_drive->strip_size, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);

            } else if (strstr(line, "Number Of Drives    :")) {
                sscanf(line, "%*s %*s %*s %*s %d", &logical_drive->drive_cnt);
            }
        }

        /* Done with MegaCLI */
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            FREE_NULL(logical_drive);
            return NULL;
        }
    }

    /* Done */
    return logical_drive;
}

/*
 * Get enclosure/slot information for given logical drive ID.
 */
int getMRLDDisks(int adapter_id, int ldrive_id, int encl_ids[], int slots[]) {
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL, *vdrive_line = NULL;
    char line[MAX_MC_LINE] = {0};
    boolean ld_start = FALSE;
    int status = 0, ld_drv_cnt = 0, encl_count = 0, slot_count = 0;

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -LdPdInfo -a%d -NoLog 2>&1",
            MEGACLI_BIN, adapter_id);
    megacli = popen(command, "r");

    /* Loop until LD is found -- then count PDs (and get data) */
    SAFE_ASPRINTF(&vdrive_line, "Virtual Drive: %d", ldrive_id);
    while (fgets(line, sizeof (line), megacli) != NULL) {
        if ((ld_start == FALSE) && (strstr(line, vdrive_line) != NULL)) {
            ld_start = TRUE;
            FREE_NULL(vdrive_line);
            vdrive_line = NULL;

        } else if (ld_start && strstr(line, "Number Of Drives    :") &&
                ld_drv_cnt == 0) {
            strtok_result = strtok(line, ":");
            strtok_result = strtok(NULL, ":");
            sscanf(strtok_result, " %d", &ld_drv_cnt);

        } else if (ld_start && strstr(line, "Enclosure Device ID:") &&
                encl_count < ld_drv_cnt) {
            strtok_result = strtok(line, ":");
            strtok_result = strtok(NULL, ":");
            sscanf(strtok_result, " %d", &encl_ids[encl_count]);
            encl_count++;

        } else if (ld_start && strstr(line, "Slot Number:") &&
                slot_count < ld_drv_cnt) {
            strtok_result = strtok(line, ":");
            strtok_result = strtok(NULL, ":");
            sscanf(strtok_result, " %d", &slots[slot_count]);
            slot_count++;
        }
    }

    /* Done with MegaCLI */
    status = pclose(megacli);
    FREE_NULL(command);
    if (status != 0) {
        return -1;
    }

    /* Done */
    return ld_drv_cnt;
}


/*
 * Get MegaRAID logical (virtual) drive properties.
 */
MRLDPROPS *getMRLDProps(int adapter_id, int ldrive_id) {
    MRLDPROPS *ld_props = 0;
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    char line[MAX_MC_LINE] = {0}, temp_str[MAX_MC_LINE] = {0};
    int status = 0;

    ld_props = (MRLDPROPS *) calloc(1, sizeof(MRLDPROPS));
    if (ld_props != NULL) {
        ld_props->adapter_id = adapter_id;
        ld_props->ldrive_id = ldrive_id;

        /* Get cache policies */
        SAFE_ASPRINTF(&command, "%s -LDGetProp -Cache -L%d -a%d -NoLog 2>&1",
                MEGACLI_BIN, ldrive_id, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Cache Policy:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strtok_result = strtok(NULL, ":");
                strtok_result = strtok(NULL, ":");
                strcpy(temp_str, strtok_result);

                /* Write policy */
                strtok_result = strtok(temp_str, ",");
                if (strstr(strStrip(strtok_result), "WriteThrough")) {
                    strncpy(ld_props->write_policy, "WT", MAX_MR_ATTR_SIZE);
                } else if (strstr(strStrip(strtok_result), "WriteBack")) {
                    strncpy(ld_props->write_policy, "WB", MAX_MR_ATTR_SIZE);
                } else {
                    strncpy(ld_props->write_policy, "UNKNOWN",
                            MAX_MR_ATTR_SIZE);
                }

                /* Read policy */
                strtok_result = strtok(NULL, ",");
                if (strstr(strStrip(strtok_result), "ReadAheadNone")) {
                    strncpy(ld_props->read_policy, "NORA", MAX_MR_ATTR_SIZE);
                } else if (strstr(strStrip(strtok_result), "ReadAhead")) {
                    strncpy(ld_props->read_policy, "RA", MAX_MR_ATTR_SIZE);
                } else if (strstr(strStrip(strtok_result), "ReadAdaptive")) {
                    strncpy(ld_props->read_policy, "ADRA", MAX_MR_ATTR_SIZE);
                } else {
                    strncpy(ld_props->read_policy, "UNKNOWN",
                            MAX_MR_ATTR_SIZE);
                }

                /* Cache policy */
                strtok_result = strtok(NULL, ",");
                if (strstr(strStrip(strtok_result), "Direct")) {
                    strncpy(ld_props->cache_policy, "Direct", MAX_MR_ATTR_SIZE);
                } else if (strstr(strStrip(strtok_result), "Cached")) {
                    strncpy(ld_props->cache_policy, "Cached", MAX_MR_ATTR_SIZE);
                } else {
                    strncpy(ld_props->cache_policy, "UNKNOWN",
                            MAX_MR_ATTR_SIZE);
                }

                /* BBU cache policy */
                strtok_result = strtok(NULL, ",");
                if (strstr(strStrip(strtok_result),
                        "No Write Cache if bad BBU")) {
                    strncpy(ld_props->bbu_cache_policy, "NoCachedBadBBU",
                            MAX_MR_ATTR_SIZE);
                } else if (strstr(strStrip(strtok_result),
                        "Write Cache OK if bad BBU")) {
                    strncpy(ld_props->bbu_cache_policy, "CachedBadBBU",
                            MAX_MR_ATTR_SIZE);
                } else {
                    strncpy(ld_props->bbu_cache_policy, "UNKNOWN",
                            MAX_MR_ATTR_SIZE);
                }
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            return NULL;
        }

        /* Get Name */
        SAFE_ASPRINTF(&command, "%s -LDGetProp -Name -L%d -a%d -NoLog 2>&1",
                MEGACLI_BIN, ldrive_id, adapter_id);
        megacli = popen(command, "r");
        while (fgets(line, sizeof(line), megacli) != NULL) {
            if (strstr(line, "Name:")) {
                strtok_result = strtok(line, ":");
                strtok_result = strtok(NULL, ":");
                strtok_result = strtok(NULL, ":");
                strtok_result = strtok(NULL, ":");
                strncpy(ld_props->name, strStrip(strtok_result),
                        MAX_MR_ATTR_SIZE);
            }
        }
        status = pclose(megacli);
        FREE_NULL(command);
        if (status != 0) {
            return NULL;
        }
    }

    /* Done */
    return ld_props;
}


/*
 * Set logical drive properties.
 */
int setMRLDProps(MRLDPROPS *ld_props) {
    char *command = NULL;
    int status = 0;

    /* Set cache policy */
    SAFE_ASPRINTF(&command, "%s -LDSetProp %s -L%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            ld_props->cache_policy, ld_props->ldrive_id, ld_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set write-cache policy */
    SAFE_ASPRINTF(&command, "%s -LDSetProp %s -L%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            ld_props->write_policy, ld_props->ldrive_id, ld_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set read-cache policy */
    SAFE_ASPRINTF(&command, "%s -LDSetProp %s -L%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            ld_props->read_policy, ld_props->ldrive_id, ld_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set BBU cache policy */
    SAFE_ASPRINTF(&command, "%s -LDSetProp %s -L%d -a%d "
            "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
            ld_props->bbu_cache_policy, ld_props->ldrive_id,
            ld_props->adapter_id);
    status = system(command);
    FREE_NULL(command);
    if (status == -1) {
        return -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }

    /* Set LD name (if not empty) */
    if (strlen(ld_props->name) != 0) {
        SAFE_ASPRINTF(&command, "%s -LDSetProp -Name %s -L%d -a%d "
                "-Silent -NoLog > /dev/null 2>&1", MEGACLI_BIN,
                ld_props->name, ld_props->ldrive_id, ld_props->adapter_id);
        status = system(command);
        FREE_NULL(command);
        if (status == -1) {
            return -1;
        } else {
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) != 0)
                    return WEXITSTATUS(status);
            } else {
                return -1;
            }
        }
    }

    /* Done */
    return 0;
}


/*
 * Check if the given logical drive is set as bootable on the adapter. This
 * will probably need to be re-worked in the future, but should provide some
 * sort of safety for now.
 */
boolean isLDBootDrive(int adapter_id, int ldrive_id) {
    FILE *megacli = NULL;
    char *command = NULL, *strtok_result = NULL;
    int status = 0, boot_ld = 0;
    char line[MAX_MC_LINE] = {0};

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -AdpBootDrive -Get -a%d -NoLog 2>&1",
            MEGACLI_BIN, adapter_id);
    megacli = popen(command, "r");

    /* Loop over command output */
    while (fgets(line, sizeof(line), megacli) != NULL) {
        if (strstr(line, "Boot Virtual Drive -")) {
            strtok_result = strtok(line, "#");
            strtok_result = strtok(NULL, "#");
            sscanf(strtok_result, "%d", &boot_ld);
        }
    }

    status = pclose(megacli);
    if (status != 0) {
        return -1;
    }

    /* Check and see if given LD is the boot drive */
    if (ldrive_id == boot_ld)
        return TRUE;
    else
        return FALSE;
}


/*
 * Delete MegaRAID logical drive.
 */
int delMRLogicalDrive(int adapter_id, int ldrive_id) {
    char *command = NULL;
    int status = 0, ret_val = 0;

    /* The delete-logical-drive command */
    SAFE_ASPRINTF(&command, "%s -CfgLdDel -L%d -a%d -Silent -NoLog > /dev/null 2>&1",
            MEGACLI_BIN, ldrive_id, adapter_id);

    /* Execute the command and check exit */
    status = system(command);
    if (status == -1) {
        ret_val = -1;
    } else {
        if (WIFEXITED(status)) {
            ret_val = WEXITSTATUS(status);
        } else {
            ret_val = -1;
        }
    }

    /* Done */
    FREE_NULL(command);
    return ret_val;
}


/*
 * Add MegaRAID logical drive.
 */
int addMRLogicalDrive(MRLDPROPS *ld_props, int num_disks, MRDISK *disks[],
        char raid_lvl[], char strip_size[]) {
    char *command = NULL, *temp_pstr = NULL;
    int status = 0, i = 0, ret_val = 0, pd_val_size = 0, pd_line_size = 0;
    char pd_list_line_buffer[MAX_MR_PD_LIST_BUFF] = {0};

    /* Build the new LD command */
    for (i = 0; i < num_disks; i++) {
        if (i == (num_disks - 1))
            SAFE_ASPRINTF(&temp_pstr, "%d:%d", disks[i]->enclosure_id,
                    disks[i]->slot_num);
        else
            SAFE_ASPRINTF(&temp_pstr, "%d:%d,", disks[i]->enclosure_id,
                disks[i]->slot_num);
        /* We add one extra for the null byte */
        pd_val_size = strlen(temp_pstr) + 1;
        pd_line_size = pd_line_size + pd_val_size;
        if (pd_line_size >= MAX_MR_PD_LIST_BUFF) {
            FREE_NULL(temp_pstr);
            return -1;
        } else {
            strcat(pd_list_line_buffer, temp_pstr);
            FREE_NULL(temp_pstr);
        }
    }
    SAFE_ASPRINTF(&command, "%s -CfgLdAdd -r%s[%s] %s %s %s %s -strpsz%s -a%d "
            "-Silent -NoLog > /dev/null 2>&1",
            MEGACLI_BIN, raid_lvl, pd_list_line_buffer, ld_props->write_policy,
            ld_props->read_policy, ld_props->cache_policy,
            ld_props->bbu_cache_policy, strip_size, ld_props->adapter_id);

    /* Execute the command and check exit */
    status = system(command);
    if (status == -1) {
        ret_val = -1;
    } else {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 254) {
                /* MegaCLI gives a exit status of 254 when using CfgLdAdd
                 * and it successfully created the volume; it gives
                 * an error about 'proc_add_new_ld: scandir failed' */
                ret_val = 0;
            } else {
                ret_val = WEXITSTATUS(status);
            }
        } else {
            ret_val = -1;
        }
    }

    /* Done */
    FREE_NULL(command);
    return ret_val;
}


/*
 * Get MegaRAID logical drive IDs.
 */
int getMRLDIDNums(int adapter_id, int ld_count, int ld_ids[]) {
    FILE *megacli = NULL;
    char *command = NULL;
    char line[MAX_MC_LINE] = {0};
    int status = 0, ld_line_cnt = 0, ret_val = 0;

    /* MegaCLI command */
    SAFE_ASPRINTF(&command, "%s -LDInfo -Lall -a%d -NoLog 2>&1",
            MEGACLI_BIN, adapter_id);
    megacli = popen(command, "r");

    /* Loop over command output */
    ld_line_cnt = 0;
    while (fgets(line, sizeof (line), megacli) != NULL) {
        if (strstr(line, "Virtual Drive:")) {
            sscanf(line, "%*s %*s %d", &ld_ids[ld_line_cnt]);
            ld_line_cnt++;
        }
    }

    /* Done with MegaCLI */
    status = pclose(megacli);
    if (status == -1) {
        ret_val = -1;
    } else {
        if (WIFEXITED(status)) {
            ret_val = WEXITSTATUS(status);
        } else {
            ret_val = -1;
        }
    }

    /* Make sure we got the same number of IDs */
    if (ld_count != ld_line_cnt)
        ret_val = -1;

    /* Done */
    FREE_NULL(command);
    return ret_val;
}

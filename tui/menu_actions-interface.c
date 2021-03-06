/**
 * @file menu_actions-interface.c
 * @author Copyright (c) 2012-2015 Astersmith, LLC
 * @author Marc A. Smith
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <cdk.h>
#include <assert.h>

#include "prototypes.h"
#include "system.h"
#include "dialogs.h"
#include "strings.h"

/*
 * Run the Help dialog
 */
void helpDialog(CDKSCREEN *main_cdk_screen) {
    CDKDIALOG *help_dialog = 0;
    char *message[HELP_MSG_SIZE] = {NULL};
    int i = 0;

    /* Set the message */
    SAFE_ASPRINTF(&message[0], "<C></31/B>ESOS TUI Help");
    SAFE_ASPRINTF(&message[1], " ");
    SAFE_ASPRINTF(&message[2], "</B>Main Menu");
    SAFE_ASPRINTF(&message[3], "To activate the main menu, hit any of the "
            "menu hot keys. You can");
    SAFE_ASPRINTF(&message[4], "then use the arrow keys to navigate the menu, "
            "and use ENTER to make");
    SAFE_ASPRINTF(&message[5], "a selection. Use ESCAPE to exit the menu without "
            "making a selection.");
    SAFE_ASPRINTF(&message[6], " ");
    SAFE_ASPRINTF(&message[7], "</B>Navigating Dialogs");
    SAFE_ASPRINTF(&message[8], "On dialogs (screens) that contain more than "
            "one widget, you can use");
    SAFE_ASPRINTF(&message[9], "TAB and SHIFT+TAB to traverse through the "
            "widgets (field entry, radio");
    SAFE_ASPRINTF(&message[10], "lists, buttons, etc.) and use ENTER to execute "
            "button functions.");
    SAFE_ASPRINTF(&message[11], "On selection dialogs, single field entry "
            "widgets, etc. you can use");
    SAFE_ASPRINTF(&message[12], "ENTER or TAB to make a choice, or use ESCAPE "
            "to cancel.");
    SAFE_ASPRINTF(&message[13], " ");
    SAFE_ASPRINTF(&message[14], "</B>Scrolling Windows");
    SAFE_ASPRINTF(&message[15], "When a scrolling window widget is active, you "
            "can use the arrow keys");
    SAFE_ASPRINTF(&message[16], "to scroll through the text and use ENTER or "
            "ESCAPE to exit.");

    while (1) {
        /* Display the TUI help message dialog box */
        help_dialog = newCDKDialog(main_cdk_screen, CENTER, CENTER, message,
                HELP_MSG_SIZE, g_ok_msg, 1, COLOR_DIALOG_SELECT,
                TRUE, TRUE, FALSE);
        if (!help_dialog) {
            errorDialog(main_cdk_screen, DIALOG_ERR_MSG, NULL);
            break;
        }
        setCDKDialogBackgroundAttrib(help_dialog, COLOR_DIALOG_TEXT);
        setCDKDialogBoxAttribute(help_dialog, COLOR_DIALOG_BOX);

        /* We don't care how the user exits the widget */
        activateCDKDialog(help_dialog, 0);
        destroyCDKDialog(help_dialog);
        break;
    }

    /* All done */
    for (i = 0; i < HELP_MSG_SIZE; i++)
        FREE_NULL(message[i]);
    refreshCDKScreen(main_cdk_screen);
    return;
}


/*
 * Run the Support Bundle dialog
 */
void supportArchDialog(CDKSCREEN *main_cdk_screen) {
    CDKDIALOG *bundle_dialog = 0;
    char tar_cmd[MAX_SHELL_CMD_LEN] = {0}, nice_date[MISC_STRING_LEN] = {0},
            bundle_file[MISC_STRING_LEN] = {0},
            file_name[MISC_STRING_LEN] = {0};
    char *error_msg = NULL;
    char *message[SUPPORT_PKG_MSG_SIZE] = {NULL};
    int ret_val = 0, exit_stat = 0, i = 0;
    time_t now = 0;
    struct tm *tm_now = NULL;

    /* Make a file name path for the support bundle */
    now = time(NULL);
    tm_now = localtime(&now);
    strftime(nice_date, sizeof(nice_date), "%s", tm_now);
    snprintf(file_name, MISC_STRING_LEN, "esos_support_pkg-%s", nice_date);
    snprintf(bundle_file, MISC_STRING_LEN, "%s/%s.tgz", TEMP_DIR, file_name);

    /* Archive the configuration files and logs */
    snprintf(tar_cmd, MAX_SHELL_CMD_LEN, "%s cpfz %s --transform "
            "'s,^,%s/,' --exclude='rc.d' --exclude='ssh' "
            "--exclude='shadow*' --exclude='ssmtp' "
            "/etc /var/log > /dev/null 2>&1", TAR_BIN, bundle_file, file_name);
    ret_val = system(tar_cmd);
    if ((exit_stat = WEXITSTATUS(ret_val)) != 0) {
        SAFE_ASPRINTF(&error_msg, CMD_FAILED_ERR, TAR_BIN, exit_stat);
        errorDialog(main_cdk_screen, error_msg, NULL);
        FREE_NULL(error_msg);
        return;
    }

    /* Set a message for the dialog */
    SAFE_ASPRINTF(&message[0], "<C></31/B>ESOS Support Bundle");
    SAFE_ASPRINTF(&message[1], " ");
    SAFE_ASPRINTF(&message[2], "<C>An archive containing configuration files "
            "and logs has been");
    SAFE_ASPRINTF(&message[3], "<C>created; its located here: </B>%s<!B>",
            bundle_file);
    SAFE_ASPRINTF(&message[4], "<C>You may now retrieve the file from this host "
            "via SFTP/SCP.");
    SAFE_ASPRINTF(&message[5], " ");
    SAFE_ASPRINTF(&message[6], " ");

    while (1) {
        /* Display the dialog box */
        bundle_dialog = newCDKDialog(main_cdk_screen, CENTER, CENTER, message,
                SUPPORT_PKG_MSG_SIZE, g_ok_msg, 1, COLOR_DIALOG_SELECT,
                TRUE, TRUE, FALSE);
        if (!bundle_dialog) {
            errorDialog(main_cdk_screen, DIALOG_ERR_MSG, NULL);
            break;
        }
        setCDKDialogBackgroundAttrib(bundle_dialog, COLOR_DIALOG_TEXT);
        setCDKDialogBoxAttribute(bundle_dialog, COLOR_DIALOG_BOX);

        /* We don't care how the user exits the widget */
        activateCDKDialog(bundle_dialog, 0);
        destroyCDKDialog(bundle_dialog);
        break;
    }

    /* All done */
    for (i = 0; i < SUPPORT_PKG_MSG_SIZE; i++)
        FREE_NULL(message[i]);
    refreshCDKScreen(main_cdk_screen);
    return;
}


/*
 * Run the About dialog
 */
void aboutDialog(CDKSCREEN *main_cdk_screen) {
    CDKDIALOG *about_dialog = 0;
    char *message[ABOUT_MSG_SIZE] = {NULL};
    char esos_ver[MAX_SYSFS_ATTR_SIZE] = {0}, hostname[MISC_STRING_LEN] = {0};
    int i = 0;

    /* Set the message */
    snprintf(esos_ver, MAX_SYSFS_ATTR_SIZE,
            "ESOS - Enterprise Storage OS %s", ESOS_VERSION);
    if (gethostname(hostname, ((sizeof hostname) - 1)) == -1)
        snprintf(hostname, sizeof (hostname), "hostname");
    SAFE_ASPRINTF(&message[0], "<C></31/B>About ESOS");
    SAFE_ASPRINTF(&message[1], " ");
    SAFE_ASPRINTF(&message[2], "</B>This Host:<!B>\t%-.40s", hostname);
    SAFE_ASPRINTF(&message[3], "</B>Running:<!B>\t%-.40s", esos_ver);
    SAFE_ASPRINTF(&message[4], " ");
    SAFE_ASPRINTF(&message[5], "</B>Build Options:<!B>\t%-.40s", BUILD_OPTS);
    SAFE_ASPRINTF(&message[6], " ");
    SAFE_ASPRINTF(&message[7], "</B>License Information");
    SAFE_ASPRINTF(&message[8], "ESOS is released under the GNU General Public "
            "License, version 3.");
    SAFE_ASPRINTF(&message[9], "QLogic Binary Firmware License: %s", QLA_FW_LICENSE);
    SAFE_ASPRINTF(&message[10], " ");
    SAFE_ASPRINTF(&message[11], " ");

    while (1) {
        /* Display the dialog box */
        about_dialog = newCDKDialog(main_cdk_screen, CENTER, CENTER, message,
                ABOUT_MSG_SIZE, g_ok_msg, 1, COLOR_DIALOG_SELECT,
                TRUE, TRUE, FALSE);
        if (!about_dialog) {
            errorDialog(main_cdk_screen, DIALOG_ERR_MSG, NULL);
            break;
        }
        setCDKDialogBackgroundAttrib(about_dialog, COLOR_DIALOG_TEXT);
        setCDKDialogBoxAttribute(about_dialog, COLOR_DIALOG_BOX);

        /* We don't care how the user exits the widget */
        activateCDKDialog(about_dialog, 0);
        destroyCDKDialog(about_dialog);
        break;
    }

    /* All done */
    for (i = 0; i < ABOUT_MSG_SIZE; i++)
        FREE_NULL(message[i]);
    refreshCDKScreen(main_cdk_screen);
    return;
}

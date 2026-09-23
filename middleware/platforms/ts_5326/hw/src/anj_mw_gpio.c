#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_file.h"
#include "anj_mw_comm.h"

#include "anj_mw_gpio.h"

int anj_gpio_read_port_value(int port)
{
    if( port <= 0 )
    {
        return -1;
    }

    int fd = -1;
    char buf[4] = {0};
    char szFileName[64] = {0};

    snprintf(szFileName, sizeof(szFileName), "/sys/class/gpio/gpio%d/value", port);

    fd = open(szFileName, O_RDONLY);
    if(fd <= 0)
    {
        __ERR("Open gpio%d failed!\n", port);
        return -1;
    }

    int len = lseek(fd, 0, SEEK_END);
    
    lseek(fd, 0, SEEK_SET);
    
    safe_read(fd, buf, len);

    close(fd);

    return atoi(buf); 
}

int anj_gpio_write_port_value(int port, int value)
{
    if( port <= 0 )
    {
        return -1;
    }

    int fd = -1;
    char filename[64] = {0};
    char cmd[4] = {0};
    snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%d/value", port);

    fd = open(filename, O_WRONLY);
    if(fd <= 0)
    {
        __ERR("filename:%s open failed!\n", filename);
        return -1;
    }			

    sprintf(cmd, "%d", value);
    safe_write(fd, cmd, strlen(cmd));

    close(fd);
	return 0;
}


int anj_gpio_write_port_direc(int port, int direc)
{
    if (port <= 0 || direc < GPIO_DIREC_IN || direc > GPIO_DIREC_OUT)
    {
        __ERR("error port:%d or error direc:%d\n", port, direc);
        return -1;
    }

    int fd = -1;
    char filename[48] = {0};
    
    snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%d/direction", port);
    fd = open(filename, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open %s failed!\n", filename);
        return -1;
    }

    char dir[10] = {0};
    snprintf(dir, sizeof(dir), "%s", GPIO_DIREC_IN == direc ? "in" : "out");

    safe_write(fd,dir,sizeof(dir));
    close(fd);

	return 0;    
}


int anj_gpio_read_port_direc(int port)
{
    if( port <= 0 )
    {
        return -1;
    }

    int fd = -1;
    char buf[10] = {0};
    char filename[48] = {0};

    snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%d/direction", port);
    fd = open(filename,O_RDONLY);
    if (fd == -1)
    {
        __ERR("open %s failed!\n", filename);
        return -1;
    }

    int len = lseek(fd, 0, SEEK_END);

    lseek(fd, 0, SEEK_SET);
    safe_read(fd, buf, len);

    close(fd);

    if (strcmp(buf, "in") == 0)
    {
        return GPIO_DIREC_IN;
    }
    else if (strcmp(buf, "out") == 0)
    {
        return GPIO_DIREC_OUT;
    }
    else
    {
        return -1;
    }
}


int anj_gpio_write_port_export(int port)
{
    if (port <= 0)
    {
        __ERR("error port:%d\n", port);
        return -1;
    }

    int fd = -1;
    char buf[16] = {0};

    snprintf(buf, sizeof(buf), "%d", port);

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
    {
        __ERR("open /sys/class/gpio/export failed!\n");
        return -1;
    }

    safe_write(fd, buf, sizeof(buf));
    close(fd);

	return 0;    
}


int anj_gpio_write_port_unexport(int port)
{
    if (port <= 0)
    {
        __ERR("error port:%d\n", port);
        return -1;
    }

    int fd = -1;
    char buf[16] = {0};

    snprintf(buf, sizeof(buf), "%d", port);

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        __ERR("open /sys/class/gpio/unexport failed!\n");
        return -1;
    }

    safe_write(fd, buf, sizeof(buf));
    close(fd);

	return 0; 
}


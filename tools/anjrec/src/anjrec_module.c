#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_module.h"

static ModuleEntry *registered_modules = NULL;

// 注册函数实现
void register_module(ModuleInterface *module)
{
    ModuleEntry *new_entry = malloc(sizeof(ModuleEntry));
    if (!new_entry)
        return;

    new_entry->info = module;
    new_entry->next = NULL;

    // 根据优先级排序插入
    if (!registered_modules || module->module_priority < registered_modules->info->module_priority)
    {
        // 插入到链表头部
        new_entry->next = registered_modules;
        registered_modules = new_entry;
    }
    else
    {
        // 查找合适的位置
        ModuleEntry *current = registered_modules;
        while (current->next && module->module_priority >= current->next->info->module_priority)
        {
            current = current->next;
        }
        new_entry->next = current->next;
        current->next = new_entry;
    }
}

// 初始化所有模块（按优先级从高到低）
int modules_init(void)
{
    ModuleEntry *current = registered_modules;
    int success = 0;

    while (current)
    {
        if (current->info->init)
        {
            // if (current->info->module_priority < MODULE_PRIORITY_SERVICE)
            {
                __INFO("module name:%s\n", current->info->module_name);
                int result = current->info->init();
                if (result != 0)
                {
                    // 处理初始化失败
                    success = -1;
                    break;
                }
            }
        }
        current = current->next;
    }

    return success;
}

// 反初始化所有模块（按优先级从低到高）
void modules_uninit(const char *skip_module, ...)
{
    if (registered_modules == NULL)
    {
        return;
    }

    // 收集要跳过的模块名
    const char **skip_list = NULL;
    size_t skip_count = 0;

    if (skip_module != NULL)
    {
        va_list ap;
        va_start(ap, skip_module);

        // 首先加入第一个参数
        skip_list = malloc(sizeof(const char *));
        if (skip_list)
        {
            skip_list[0] = skip_module;
            skip_count = 1;

            const char *s = NULL;
            while ((s = va_arg(ap, const char *)) != NULL)
            {
                const char **tmp = realloc(skip_list, (skip_count + 1) * sizeof(const char *));
                if (!tmp)
                {
                    // 内存分配失败，停止收集更多 skip 项
                    break;
                }
                skip_list = tmp;
                skip_list[skip_count++] = s;
            }
        }

        va_end(ap);
    }

    // 先反转链表，然后反初始化
    ModuleEntry *prev = NULL;
    ModuleEntry *current = registered_modules;
    ModuleEntry *next = NULL;

    // 反转链表
    while (current)
    {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    registered_modules = prev;

    // 反初始化（现在从低优先级到高优先级）
    ModuleEntry *remaining = NULL;

    current = registered_modules;
    while (current)
    {
        int should_skip = 0;

        next = current->next;
        if (skip_count > 0 && skip_list)
        {
            size_t i = 0;
            for (i = 0; i < skip_count; ++i)
            {
                if (skip_list[i] && strcmp(current->info->module_name, skip_list[i]) == 0)
                {
                    should_skip = 1;
                    break;
                }
            }
        }

        if (should_skip)
        {
            current->next = remaining;
            remaining = current;
        }
        else
        {
            if (current->info->uninit)
            {
                __INFO("module name:%s\n", current->info->module_name);
                current->info->uninit();
            }
            free(current);
        }
        current = next;
    }

    registered_modules = remaining;
    free(skip_list);
}

int module_init_single(const char *module_name)
{
    if (!module_name || !registered_modules)
    {
        return -1;
    }

    ModuleEntry *current = registered_modules;
    while (current)
    {
        if (strcmp(current->info->module_name, module_name) == 0)
        {
            if (current->info->init)
            {
                __INFO("module name:%s init\n", current->info->module_name);
                int result = current->info->init();
                if (result != 0)
                {
                    __ERR("Module %s init failed: %d\n", module_name, result);
                    return -1;
                }
                return 0;
            }
            else
            {
                __WARN("Module %s has no init function\n", module_name);
                return -1;
            }
        }
        current = current->next;
    }

    __WARN("Module %s not found\n", module_name);
    return -1;
}

int module_uninit_single(const char *module_name)
{
    if (!module_name || !registered_modules)
    {
        return -1;
    }

    ModuleEntry *current = registered_modules;

    while (current)
    {
        if (strcmp(current->info->module_name, module_name) == 0)
        {
            if (current->info->uninit)
            {
                int result = current->info->uninit();
                if (result != 0)
                {
                    __ERR("Module %s uninit failed: %d\n", module_name, result);
                    return -1;
                }
            }

            __INFO("Module %s uninit successfully\n", module_name);
            return 0;
        }
        current = current->next;
    }

    __WARN("Module %s not found\n", module_name);
    return -1;
}

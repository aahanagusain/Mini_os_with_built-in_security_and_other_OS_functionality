#include "../include/netsec.h"
#include "../include/memory.h"

#define MAX_RULES 32

static struct fw_rule rules[MAX_RULES];
static int rule_count = 0;
static int next_rule_id = 1;

int netsec_init(void)
{
    rule_count = 0;
    next_rule_id = 1;
    /* Add default deny rule */
    struct fw_rule deny = {
        .type = RULE_DENY,
        .target_type = TARGET_ANY,
        .port = 0,
        .address = 0,
        .mask = 0
    };
    return netsec_add_rule(&deny);
}

int netsec_add_rule(const struct fw_rule *rule)
{
    if (!rule || rule_count >= MAX_RULES) return -1;
    rules[rule_count] = *rule;
    rule_count++;
    return next_rule_id++;
}

int netsec_remove_rule(int rule_id)
{
    for (int i = 0; i < rule_count; i++) {
        if (i == rule_id - 1) {
            /* Shift remaining rules down */
            for (int j = i; j < rule_count - 1; j++) {
                rules[j] = rules[j + 1];
            }
            rule_count--;
            return 0;
        }
    }
    return -1;
}

int netsec_list_rules(struct fw_rule *out, int max_rules)
{
    if (!out || max_rules <= 0) return -1;
    int count = (rule_count < max_rules) ? rule_count : max_rules;
    for (int i = 0; i < count; i++) {
        out[i] = rules[i];
    }
    return count;
}

int netsec_check_connection(uint32_t addr, uint16_t port)
{
    /* Check rules in order */
    for (int i = 0; i < rule_count; i++) {
        const struct fw_rule *r = &rules[i];
        int match = 0;
        
        switch (r->target_type) {
            case TARGET_ANY:
                match = 1;
                break;
            case TARGET_PORT:
                match = (port == r->port);
                break;
            case TARGET_ADDRESS:
                /* Check if addr is in network */
                match = ((addr & r->mask) == (r->address & r->mask));
                break;
        }
        
        if (match) {
            return (r->type == RULE_ALLOW) ? 1 : 0;
        }
    }
    
    return 0; /* Implicit deny */
}
#ifndef NETSEC_H
#define NETSEC_H

#include <stdint.h>

/* Network firewall rule types */
#define RULE_ALLOW 1
#define RULE_DENY  2

/* Rule targets */
#define TARGET_ANY 0
#define TARGET_PORT 1
#define TARGET_ADDRESS 2
#define MAX_RULES 32

/* Simple firewall rule structure */
struct fw_rule {
    uint8_t type;          /* RULE_ALLOW or RULE_DENY */
    uint8_t target_type;   /* TARGET_* */
    uint16_t port;         /* Port number if TARGET_PORT */
    uint32_t address;      /* IPv4 address if TARGET_ADDRESS */
    uint32_t mask;         /* Network mask if TARGET_ADDRESS */
};

/* Initialize network security subsystem */
int netsec_init(void);

/* Add a firewall rule. Returns rule ID or negative on error */
int netsec_add_rule(const struct fw_rule *rule);

/* Remove a rule by ID */
int netsec_remove_rule(int rule_id);

/* List rules. Returns number of rules copied */
int netsec_list_rules(struct fw_rule *rules, int max_rules);

/* Check if a connection would be allowed */
int netsec_check_connection(uint32_t addr, uint16_t port);

#endif /* NETSEC_H */
/* Modified by: xChaos, 20121007 */

#include "cll1-0.6.2.h"
#include "ipstruct.h"

#define FIRSTGROUPID 1024
#define FIRSTIPCLASS 2048

/* globals declared in prometheus.c */
extern struct IP *ips, *ip, *sharedip;
extern struct Group *groups, *group;
extern struct Keyword *keyword, *defaultkeyword, *keywords;
extern int class_count;
extern int ip_count;
extern int found_lmsid;
extern int free_min;
extern const int highest_priority;
extern char *ip6prefix;

/* This must be object oriented! This looks almost like constructor ;-) */
void TheIP(char *ipaddr)
{
 create(ip,IP);
 ip->name        = "";
 ip->addr        = ipaddr;
 ip->sharing     = NULL;
 ip->prio        = highest_priority+1;
 ip->lmsid       = -1;
 ip->fixedprio   = \
 ip->mark        = \
 ip->min         = \
 ip->max         = \
 ip->desired     = \
 ip->credit      = \
 ip->upload      = \
 ip->proxy       = \
 ip->direct      = \
 ip->traffic     = \
 ip->pktsup      = \
 ip->pktsdown    = 0;
 ip->keyword     = keywords;
 ip->v6          = (strchr(ip->addr,':')!=NULL);
 push(ip,ips);
 ip_count++;
}

struct IP *lastIP6;

/* == This function strips extra characters after IPv4 address and stores it = */
parse_ip(char *str)
{
 char *ptr, *ipaddr, *ip6range = NULL, *ipname = NULL, *lmsid = NULL;

 if(ip6prefix) /* Try this only if IPv6 subsystem is active... */
 {
  ptr = strstr(str, "::");
  if(ptr && ptr-str > 4)
  {
   ptr -= 4;   
   duplicate(ptr, ip6range);
   ptr = strstr(ip6range, "::");
   if(ptr)
   {
    *(ptr+2) = 0;
   }
  }
 }

 ptr = strchr(str, '{');
 if(ptr)
 {
  lmsid = ++ptr;
  while(*ptr and *ptr != '}')
  {
   ptr++;
  }
  *ptr = 0;
 }
 
 ptr = str;
 while(*ptr and *ptr!=' ' and *ptr!=9)
 {
  ptr++;
 }
 
 *ptr = 0;
 ipaddr = str;
 ptr++;
 while(*ptr and (*ptr==' ' or *ptr==9))
 {
  ptr++;
 }
 ipname=ptr; 
 while(*ptr and *ptr!=' ' and *ptr!=9)
 {
  ptr++;
 }
 *ptr=0;

 if(ip6range)
 {
  concatenate(ip6prefix,ip6range,ptr);
  ip6range=ptr;
  if_exists(ip, ips, eq(ip->addr,ip6range));
  else
  {
   TheIP(ip6range);
  }
  ip->name = ip6range;
  ip->keyword = defaultkeyword; /* settings for default keyword */
  if(lmsid)
  {
   ip->lmsid = atoi(lmsid);
  }
  lastIP6 = ip;
 }
 else
 {
  lastIP6 = NULL;
 }

 if_exists(ip, ips, eq(ip->addr,ipaddr));
 else
 {
  TheIP(ipaddr);
 }
 ip->name = ipname;
 if(lmsid)
 {
  ip->lmsid = atoi(lmsid);
  found_lmsid = TRUE;
 }
}

/* == This function parses hosts style main configuration file == */
void parse_hosts(char *hosts)
{
 int groupidx = FIRSTGROUPID;
 char *str, *ptr;
 char *substring;

 parse(hosts)
 {
  str=_;

  if(*str < '0' or *str > '9')
  {
   /* any line starting with non-number is comment ...*/
   continue;
  }
  
  /* Does this IP share QoS class with some other ? */
  substring = strstr(str, "sharing-");
  if(substring)
  { 
   substring += 8; /* "sharing-" */
   parse_ip(str);
   ip->sharing = substring;
   ip->keyword = defaultkeyword; /* settings for default keyword */
   if(lastIP6)
   {
    lastIP6->sharing = substring;
    lastIP6 = NULL;
   }
   while(*substring and *substring != '\n')
   {
    substring++;
   }
   *substring = 0; 
  }
  else
  {
   /*Do we have to create new QoS class for this IP ? */

   if_exists(keyword,keywords,(substring=strstr(str,keyword->key)))
   {
    parse_ip(str);
    if(lastIP6)
    {
     lastIP6->sharing = ip->name;
     lastIP6 = NULL;
    }
    ip->keyword = keyword;
    keyword->ip_count++;
    ip->prio = keyword->default_prio;
    substring += strlen(keyword->key)+1;
    ptr = substring;
    while(*ptr and *ptr != '-')
    {
     ptr++;
    }
    if(*ptr == '-')
    {
     *ptr=0;
     ip->max = ip->desired = atoi(ptr+1);
    }
    ip->min = atoi(substring);
    if(ip->min <= 0)
    {
     printf(" %s: Illegal value of minimum bandwidth 0 kbps, using %d kb/s\n",
            str, free_min);
     ip->min = free_min;
    }
    if(ip->max <= ip->min)
    {
     ip->fixedprio = TRUE;
     ip->max = ip->min + ip->keyword->reserve_min;
    }
    else 
    {
     ip->max -= ip->keyword->reserve_max;
     if(ip->max<ip->min)
     {
      ip->max=ip->min;
     }
    }
    ip->mark = FIRSTIPCLASS+1+class_count++;

    if_exists(group,groups,(group->min == ip->min)) 
    { 
     group->count++;      
     group->desired += ip->min;
     ip->group = group->id;   
    }
    else
    {
     create(group,Group);
     group->min = ip->min;
     group->id = groupidx++;
     ip->group = group->id;

     if(group->min < 8) group->min = 8;
     /* Warning - this is maybe because of primitive tc namespace, can be fixed */
     /* it is because class IDs are derived from min. bandwidth. - xCh */
     //if(group->min>MAX_GUARANTED_KBPS) group->min=MAX_GUARANTED_KBPS;
     
     group->count = 1;
     group->desired = ip->min;   
     insert(group, groups, desc_order_by,min);
    }
   }//endif keyword-
  }//endif sharing-
 }
 fail
 {
  perror(hosts);
  exit(-1);
 }
 done; /* ugly macro end */
}
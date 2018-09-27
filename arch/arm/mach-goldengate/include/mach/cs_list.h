/*
   This double linked list implementation is based on Rusty Russell's CCAN version

   See https://github.com/rustyrussell/ccan for details

   The macros from this header file can be used similarly to Linux's linked lists

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#define cs_container_off(containing_type, member)	\
        offsetof(containing_type, member)

#define cs_container_of(member_ptr, containing_type, member)		\
         ((containing_type *)						\
          ((char *)(member_ptr)						\
           - cs_container_off(containing_type, member))			\
          + check_types_match(*(member_ptr), ((containing_type *)0)->member))

#define cs_container_of_var(member_ptr, container_var, member)	\
	((void *)((char *)(member_ptr)				\
		  - ((char *)&(container_var)->member		\
		     - (char *)(container_var))))

struct cs_list_node {
	struct cs_list_node *next, *prev;
};

struct cs_list_head {
        struct cs_list_node n;
};

#define CS_LIST_HEAD_INIT(name) { { &name.n, &name.n } }

#define CS_LIST_HEAD(name)	\
        struct cs_list_head name = CS_LIST_HEAD_INIT(name)

static inline void cs_list_head_init(struct cs_list_head *h)
{
        h->n.next = h->n.prev = &h->n;
}

static inline void cs_list_add(struct cs_list_head *h, struct cs_list_node *n)
{
        n->next = h->n.next;
        n->prev = &h->n;
        h->n.next->prev = n;
        h->n.next = n;
}

static inline void cs_list_add_tail(struct cs_list_head *h, struct cs_list_node *n)
{
        n->next = &h->n;
        n->prev = h->n.prev;
        h->n.prev->next = n;
        h->n.prev = n;
}

static inline int cs_list_empty(const struct cs_list_head *h)
{
        return h->n.next == &h->n;
}

static inline void cs_list_del(struct cs_list_node *n)
{
        n->next->prev = n->prev;
        n->prev->next = n->next;
}

static inline void cs_list_del_from(struct cs_list_head *h, struct cs_list_node *n)
{
        if (cs_list_empty(h))
		return;
        cs_list_del(n);
}

#define cs_list_entry(n, type, member) container_of(n, type, member)

#define cs_list_for_each(h, i, member)				\
        for (i = cs_container_of_var((h)->n.next, i, member);	\
             &i->member != &(h)->n;				\
             i = cs_container_of_var(i->member.next, i, member))

#define cs_list_for_each_rev(h, i, member)			\
        for (i = cs_container_of_var((h)->n.prev, i, member);	\
             &i->member != &(h)->n;				\
             i = cs_container_of_var(i->member.prev, i, member))

#define cs_list_for_each_safe(h, i, nxt, member)			\
        for (i = cs_container_of_var((h)->n.next, i, member),		\
                nxt = cs_container_of_var(i->member.next, i, member);	\
             &i->member != &(h)->n;					\
             i = nxt, nxt = cs_container_of_var(i->member.next, i, member))

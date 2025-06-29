NAME = ft_ping

SRCS_DIR = srcs
SRCS = $(wildcard $(SRCS_DIR)/*.c)

RM = rm -f
CFLAGS = -g -Wall -Wextra -Werror -Wshadow
SFLAGS = -fsanitize=address
C = cc
INCLUDES = -I includes
HDRS = $(wildcard includes/*.h)
OBJS = $(addprefix $(OBJS_DIR)/,$(SRCS:srcs/%.c=%.o))
SOBJS = $(addprefix $(OBJS_DIR_S)/,$(SRCS:srcs/%.c=%.o))

OBJS_DIR = objs
OBJS_DIR_S = s_objs

# Color codes
GREEN = \033[0;32m
RED = \033[0;31m
BLUE = \033[0;34m 
ORANGE = \033[0;33m
NC = \033[0m 

all: $(NAME)

clean:
	@$(RM) -r $(OBJS_DIR)
	@$(RM) -r $(OBJS_DIR_S)
	@echo "$(RED)$(NAME)$(NC)OBJS cleaned!"

fclean: clean
	@$(RM) $(NAME)
	@$(RM) $(BONUS_NAME)
	@echo "$(RED)$(NAME)$(NC)cleaned!"

re: fclean all

$(OBJS_DIR)/%.o: srcs/%.c $(HDRS)
	@mkdir -p $(dir $@)
	@$(C) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJS_DIR_S)/%.o: srcs/%.c $(HDRS)
	@mkdir -p $(dir $@)
	@$(C) $(CFLAGS) $(SFLAGS) $(INCLUDES) -c $< -o $@

$(NAME): $(OBJS)
	@echo "$(GREEN)$(NAME)$(NC) compiling..."
	@$(C) $(CFLAGS) -o $(NAME) $(OBJS) $(INCLUDES)
	@echo "$(GREEN)$(NAME)$(NC) ready!"

v: 
	make re && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes ./$(NAME)

.PHONY: all fclean clean re v 
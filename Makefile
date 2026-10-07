NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude

SRCDIR = src
SRC = main.cpp $(SRCDIR)/Config.cpp $(SRCDIR)/ConfigParser.cpp $(SRCDIR)/Location.cpp $(SRCDIR)/ServerConfig.cpp\
	$(SRCDIR)/ServerSocket.cpp $(SRCDIR)/NetworkManager.cpp $(SRCDIR)/Client.cpp \
	$(SRCDIR)/RequestHandler.cpp $(SRCDIR)/RequestRouting.cpp $(SRCDIR)/RequestResources.cpp \
	$(SRCDIR)/RequestUtils.cpp $(SRCDIR)/RootedPath.cpp $(SRCDIR)/RequestCgi.cpp \
	$(SRCDIR)/CgiProcess.cpp $(SRCDIR)/CgiRequest.cpp $(SRCDIR)/CgiContext.cpp $(SRCDIR)/MultipartUpload.cpp \
	$(SRCDIR)/HttpParser.cpp $(SRCDIR)/HttpRequest.cpp \
	$(SRCDIR)/HttpResponse.cpp $(SRCDIR)/ResponseBuilder.cpp


OBJDIR = out
OBJ = $(addprefix $(OBJDIR)/,$(SRC:.cpp=.o))
DEP = $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEP)

.PHONY: all clean fclean re

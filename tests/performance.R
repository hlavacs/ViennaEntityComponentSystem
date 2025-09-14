# Load required libraries
library(ggplot2)
library(dplyr)


df <- read.csv("out.csv", fileEncoding = "UTF-16",header=TRUE,sep=",", stringsAsFactors =TRUE)
all_data <- df

# Calculate summary statistics for confidence intervals
summary_data <- all_data %>%
  group_by(x, dataset, subgroup) %>%
  summarise(
    mean_y = mean(y),
    se_y = sd(y) / sqrt(n()),
    n = n(),
    .groups = 'drop'
  ) %>%
  mutate(
    # 95% confidence interval
    ci_lower = mean_y - qt(0.975, df = n - 1) * se_y,
    ci_upper = mean_y + qt(0.975, df = n - 1) * se_y
  )

# Method 1: All datasets and subgroups on one plot
p1 <- ggplot(summary_data, aes(x = x, color = dataset, linetype = subgroup)) +
  #geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper, fill = dataset), alpha = 0.2) +
  geom_line(aes(y = mean_y), size = 0.8) +
  geom_point(aes(y = mean_y), size = 1.5) +
  labs(title = "Avg. runtime depending on #Components with Seq/Rnd Subgroups",
       #subtitle = "Lines show means", #, ribbons show 95% confidence intervals",
       x = "X Variable", y = "Y Variable",
       color = "Dataset", fill = "Dataset", linetype = "Subgroup") +
  theme_minimal() +
  theme_bw(base_size = 16) +
  theme(legend.position = "right") + 
  scale_x_log10() +
  scale_y_log10() +
  labs(x = "Number of Entities", y = "Time (us)")


# Display plots
print(p1)
ggsave("my_plot.png", plot = p1, bg = "white", width = 10, height = 6, dpi = 300)



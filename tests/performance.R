# Load required libraries
library(ggplot2)
library(dplyr)

# Create sample data: 10 datasets, each with SEQ and RND subgroups
set.seed(123)

# Function to generate data for one dataset
generate_dataset <- function(dataset_id, n_points_per_x = 15) {
  x_values <- rep(1:10, each = n_points_per_x)
  
  # SEQ subgroup - more structured/sequential pattern
  seq_data <- data.frame(
    x = x_values,
    y = 2 * x_values + dataset_id * 3 + rnorm(length(x_values), 0, 1.5),
    dataset = as.factor(dataset_id),
    subgroup = "SEQ"
  )
  
  # RND subgroup - more random pattern
  rnd_data <- data.frame(
    x = x_values,
    y = 1.5 * x_values + dataset_id * 2.5 + rnorm(length(x_values), 0, 2.5),
    dataset = as.factor(dataset_id),
    subgroup = "RND"
  )
  
  return(rbind(seq_data, rnd_data))
}

# Generate only datasets 1, 2, 4, 6, 8, 10
selected_dataset_ids <- c(1, 2, 4, 6, 8, 10)
all_data <- do.call(rbind, lapply(selected_dataset_ids, generate_dataset))

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
  geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper, fill = dataset), alpha = 0.2) +
  geom_line(aes(y = mean_y), size = 0.8) +
  geom_point(aes(y = mean_y), size = 1.5) +
  labs(title = "Selected 6 Datasets with SEQ/RND Subgroups - Combined View",
       subtitle = "Lines show means, ribbons show 95% confidence intervals",
       x = "X Variable", y = "Y Variable",
       color = "Dataset", fill = "Dataset", linetype = "Subgroup") +
  theme_minimal() +
  theme(legend.position = "right")

# Method 2: Faceted by dataset
p2 <- ggplot(summary_data, aes(x = x, color = subgroup, fill = subgroup)) +
  geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper), alpha = 0.3) +
  geom_line(aes(y = mean_y), size = 1) +
  geom_point(aes(y = mean_y), size = 2) +
  facet_wrap(~dataset, nrow = 2, labeller = label_both) +
  scale_color_manual(values = c("SEQ" = "blue", "RND" = "red")) +
  scale_fill_manual(values = c("SEQ" = "blue", "RND" = "red")) +
  labs(title = "6 Datasets Faceted - SEQ vs RND Comparison",
       subtitle = "Each panel shows one dataset with both subgroups",
       x = "X Variable", y = "Y Variable",
       color = "Subgroup", fill = "Subgroup") +
  theme_minimal() +
  theme(legend.position = "bottom")

# Method 3: Faceted by subgroup
p3 <- ggplot(summary_data, aes(x = x, color = dataset, fill = dataset)) +
  geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper), alpha = 0.3) +
  geom_line(aes(y = mean_y), size = 0.8) +
  geom_point(aes(y = mean_y), size = 1.5) +
  facet_wrap(~subgroup, labeller = label_both) +
  labs(title = "SEQ vs RND Subgroups - Selected 6 Datasets by Subgroup",
       subtitle = "Left: SEQ subgroup, Right: RND subgroup",
       x = "X Variable", y = "Y Variable",
       color = "Dataset", fill = "Dataset") +
  theme_minimal() +
  theme(legend.position = "bottom")

# Method 4: Focus on subset of these datasets (1, 4, 8)
focused_datasets <- c("1", "4", "8")
subset_data <- summary_data %>% filter(dataset %in% focused_datasets)

p4 <- ggplot(subset_data, aes(x = x, color = interaction(dataset, subgroup))) +
  geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper, 
                  fill = interaction(dataset, subgroup)), alpha = 0.2) +
  geom_line(aes(y = mean_y), size = 1) +
  geom_point(aes(y = mean_y), size = 2) +
  scale_color_discrete(name = "Dataset.Subgroup") +
  scale_fill_discrete(name = "Dataset.Subgroup") +
  labs(title = "Focused View: Datasets (1, 4, 8) with Confidence Intervals",
       subtitle = "Both SEQ and RND subgroups shown",
       x = "X Variable", y = "Y Variable") +
  theme_minimal() +
  theme(legend.position = "right")

# Method 5: Error bars instead of ribbons
p5 <- ggplot(summary_data, aes(x = x, y = mean_y, color = subgroup)) +
  geom_errorbar(aes(ymin = ci_lower, ymax = ci_upper), 
                width = 0.2, alpha = 0.7) +
  geom_point(size = 2) +
  geom_line(alpha = 0.8) +
  facet_wrap(~dataset, nrow = 2, labeller = label_both) +
  scale_color_manual(values = c("SEQ" = "blue", "RND" = "red")) +
  labs(title = "Error Bar Representation - All Datasets",
       subtitle = "Error bars show 95% confidence intervals",
       x = "X Variable", y = "Y Variable",
       color = "Subgroup") +
  theme_minimal() +
  theme(legend.position = "bottom")

# Method 6: Comparison plot - SEQ vs RND differences (manual calculation)
# Split data by subgroup manually
seq_data <- summary_data %>% filter(subgroup == "SEQ")
rnd_data <- summary_data %>% filter(subgroup == "RND")

# Merge on x and dataset to calculate differences
comparison_data <- merge(seq_data, rnd_data, by = c("x", "dataset"), suffixes = c("_SEQ", "_RND"))

# Calculate differences and propagate uncertainty
comparison_data <- comparison_data %>%
  mutate(
    diff_mean = mean_y_SEQ - mean_y_RND,
    # Approximate standard error for difference
    diff_se = sqrt(se_y_SEQ^2 + se_y_RND^2),
    diff_ci_lower = diff_mean - 1.96 * diff_se,
    diff_ci_upper = diff_mean + 1.96 * diff_se
  )

p6 <- ggplot(comparison_data, aes(x = x, y = diff_mean, color = dataset, fill = dataset)) +
  geom_ribbon(aes(ymin = diff_ci_lower, ymax = diff_ci_upper), alpha = 0.3) +
  geom_line(size = 1) +
  geom_point(size = 2) +
  geom_hline(yintercept = 0, linetype = "dashed", color = "black") +
  labs(title = "SEQ - RND Differences with Confidence Intervals",
       subtitle = "Positive values: SEQ > RND, Negative values: RND > SEQ",
       x = "X Variable", y = "Difference (SEQ - RND)",
       color = "Dataset", fill = "Dataset") +
  theme_minimal() +
  theme(legend.position = "bottom")

# Method 7: Side-by-side comparison without faceting
# Create separate plots for SEQ and RND
seq_summary <- summary_data %>% filter(subgroup == "SEQ")
rnd_summary <- summary_data %>% filter(subgroup == "RND")

p7 <- ggplot() +
  # SEQ data
  geom_ribbon(data = seq_summary, 
              aes(x = x, ymin = ci_lower, ymax = ci_upper, fill = dataset), 
              alpha = 0.2) +
  geom_line(data = seq_summary, 
            aes(x = x, y = mean_y, color = dataset), 
            size = 1, linetype = "solid") +
  geom_point(data = seq_summary, 
             aes(x = x, y = mean_y, color = dataset), 
             size = 2, shape = 16) +
  # RND data  
  geom_ribbon(data = rnd_summary, 
              aes(x = x, ymin = ci_lower, ymax = ci_upper, fill = dataset), 
              alpha = 0.1) +
  geom_line(data = rnd_summary, 
            aes(x = x, y = mean_y, color = dataset), 
            size = 1, linetype = "dashed") +
  geom_point(data = rnd_summary, 
             aes(x = x, y = mean_y, color = dataset), 
             size = 2, shape = 17) +
  labs(title = "SEQ vs RND Overlay - Different Line Styles",
       subtitle = "Solid lines/circles: SEQ, Dashed lines/triangles: RND",
       x = "X Variable", y = "Y Variable",
       color = "Dataset", fill = "Dataset") +
  theme_minimal() +
  theme(legend.position = "bottom")

# Display plots
print(p1)
print(p2)
print(p3)
print(p4)
print(p5)
print(p6)
print(p7)

# Function to plot specific datasets with confidence intervals (no tidyr)
plot_datasets_with_subgroups <- function(data, datasets = NULL, style = "ribbon", 
                                         conf_level = 0.95, facet_by = "dataset") {
  
  # Filter datasets if specified
  if (!is.null(datasets)) {
    data <- data %>% filter(dataset %in% as.character(datasets))
  }
  
  # Calculate confidence intervals
  alpha <- 1 - conf_level
  summary_df <- data %>%
    group_by(x, dataset, subgroup) %>%
    summarise(
      mean_y = mean(y),
      se_y = sd(y) / sqrt(n()),
      n = n(),
      .groups = 'drop'
    ) %>%
    mutate(
      ci_lower = mean_y - qt(1 - alpha/2, df = n - 1) * se_y,
      ci_upper = mean_y + qt(1 - alpha/2, df = n - 1) * se_y
    )
  
  # Create base plot
  if (style == "ribbon") {
    p <- ggplot(summary_df, aes(x = x, color = subgroup, fill = subgroup)) +
      geom_ribbon(aes(ymin = ci_lower, ymax = ci_upper), alpha = 0.3) +
      geom_line(aes(y = mean_y), size = 1) +
      geom_point(aes(y = mean_y), size = 2)
  } else if (style == "errorbar") {
    p <- ggplot(summary_df, aes(x = x, y = mean_y, color = subgroup)) +
      geom_errorbar(aes(ymin = ci_lower, ymax = ci_upper), width = 0.2) +
      geom_point(size = 2) +
      geom_line()
  }
  
  # Add faceting
  if (facet_by == "dataset") {
    p <- p + facet_wrap(~dataset, labeller = label_both)
  } else if (facet_by == "subgroup") {
    p <- p + facet_wrap(~subgroup, labeller = label_both)
  }
  
  p <- p +
    scale_color_manual(values = c("SEQ" = "blue", "RND" = "red")) +
    scale_fill_manual(values = c("SEQ" = "blue", "RND" = "red")) +
    labs(title = paste("Datasets with", scales::percent(conf_level), "Confidence Intervals"),
         x = "X Variable", y = "Y Variable") +
    theme_minimal() +
    theme(legend.position = "bottom")
  
  return(p)
}

# Summary statistics
cat("Summary by Dataset and Subgroup (Datasets 1, 2, 4, 6, 8, 10):\n")
all_data %>%
  group_by(dataset, subgroup) %>%
  summarise(
    n_points = n(),
    mean_y = round(mean(y), 2),
    sd_y = round(sd(y), 2),
    .groups = 'drop'
  ) %>%
  print()

# Alternative summary using base R approach
cat("\nAlternative Summary for Selected Datasets (base R style):\n")
summary_table <- aggregate(y ~ dataset + subgroup, data = all_data, 
                           FUN = function(x) c(n = length(x), 
                                               mean = round(mean(x), 2), 
                                               sd = round(sd(x), 2)))
print(summary_table)

# Example usage:
# plot_datasets_with_subgroups(all_data, datasets = c(1, 3, 5), 
#                             style = "ribbon", facet_by = "dataset")
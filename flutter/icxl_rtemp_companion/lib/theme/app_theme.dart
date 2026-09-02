import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

/// Apple-HIG-inspired visual tokens for ICXL-RTemp companion.
abstract final class AppTokens {
  // Light
  static const Color lightScaffold = Color(0xFFF2F2F7); // secondarySystemBackground
  static const Color lightCard = Color(0xFFFFFFFF); // systemBackground
  static const Color lightSeparator = Color(0xFFC6C6C8);

  // Dark — never pure black fills
  static const Color darkScaffold = Color(0xFF1C1C1E);
  static const Color darkCard = Color(0xFF2C2C2E);
  static const Color darkSeparator = Color(0xFF38383A);

  // Accent + soft semantics
  static const Color systemBlue = Color(0xFF007AFF);
  static const Color semanticGreen = Color(0xFF34C759);
  static const Color semanticAmber = Color(0xFFFF9500);
  static const Color semanticRed = Color(0xFFFF3B30);
  static const Color secondaryLabelLight = Color(0xFF8E8E93);
  static const Color secondaryLabelDark = Color(0xFF8E8E93);

  static const double largeTitleSize = 34;
  static const double cardTitleSize = 17;
  static const double bodySize = 17;
  static const double secondarySize = 15;
  static const double footnoteSize = 13;

  static const double cardRadius = 12;
  static const double pageHorizontal = 16;
  static const double betweenCards = 16;
  static const double cardPadding = 12;
  static const double minTouch = 44;
}

ThemeData buildAppTheme(Brightness brightness) {
  final isDark = brightness == Brightness.dark;
  final seed = AppTokens.systemBlue;
  final scheme = ColorScheme.fromSeed(
    seedColor: seed,
    brightness: brightness,
    primary: AppTokens.systemBlue,
    surface: isDark ? AppTokens.darkCard : AppTokens.lightCard,
  ).copyWith(
    surfaceContainerLowest: isDark ? AppTokens.darkScaffold : AppTokens.lightScaffold,
    error: AppTokens.semanticRed,
  );

  final scaffold = isDark ? AppTokens.darkScaffold : AppTokens.lightScaffold;
  final card = isDark ? AppTokens.darkCard : AppTokens.lightCard;
  final secondary =
      isDark ? AppTokens.secondaryLabelDark : AppTokens.secondaryLabelLight;

  final textTheme = TextTheme(
    displayLarge: TextStyle(
      fontSize: AppTokens.largeTitleSize,
      fontWeight: FontWeight.bold,
      letterSpacing: 0.37,
      height: 1.2,
      color: scheme.onSurface,
    ),
    titleMedium: TextStyle(
      fontSize: AppTokens.cardTitleSize,
      fontWeight: FontWeight.w600,
      color: scheme.onSurface,
    ),
    bodyLarge: TextStyle(
      fontSize: AppTokens.bodySize,
      fontWeight: FontWeight.w400,
      color: scheme.onSurface,
    ),
    bodyMedium: TextStyle(
      fontSize: AppTokens.secondarySize,
      fontWeight: FontWeight.w400,
      color: secondary,
    ),
    bodySmall: TextStyle(
      fontSize: AppTokens.footnoteSize,
      fontWeight: FontWeight.w400,
      color: secondary,
    ),
  );

  return ThemeData(
    useMaterial3: true,
    brightness: brightness,
    colorScheme: scheme,
    scaffoldBackgroundColor: scaffold,
    canvasColor: scaffold,
    cardColor: card,
    dividerColor: isDark ? AppTokens.darkSeparator : AppTokens.lightSeparator,
    textTheme: textTheme,
    primaryTextTheme: textTheme,
    appBarTheme: AppBarTheme(
      backgroundColor: scaffold,
      foregroundColor: scheme.onSurface,
      elevation: 0,
      scrolledUnderElevation: 0,
      centerTitle: false,
      titleTextStyle: textTheme.titleMedium,
    ),
    cardTheme: CardThemeData(
      color: card,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(AppTokens.cardRadius),
      ),
    ),
    listTileTheme: ListTileThemeData(
      minVerticalPadding: 12,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      iconColor: AppTokens.systemBlue,
      textColor: scheme.onSurface,
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: AppTokens.systemBlue,
        foregroundColor: Colors.white,
        minimumSize: const Size(64, AppTokens.minTouch),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        textStyle: const TextStyle(
          fontSize: AppTokens.bodySize,
          fontWeight: FontWeight.w600,
        ),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: AppTokens.systemBlue,
        minimumSize: const Size(64, AppTokens.minTouch),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
      ),
    ),
    textButtonTheme: TextButtonThemeData(
      style: TextButton.styleFrom(
        foregroundColor: AppTokens.systemBlue,
        minimumSize: const Size(44, AppTokens.minTouch),
      ),
    ),
    snackBarTheme: SnackBarThemeData(
      behavior: SnackBarBehavior.floating,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
    ),
    cupertinoOverrideTheme: CupertinoThemeData(
      brightness: brightness,
      primaryColor: AppTokens.systemBlue,
      scaffoldBackgroundColor: scaffold,
      barBackgroundColor: scaffold,
    ),
  );
}

Color cardBackground(BuildContext context) {
  final brightness = Theme.of(context).brightness;
  return brightness == Brightness.dark
      ? AppTokens.darkCard
      : AppTokens.lightCard;
}

Color scaffoldBackground(BuildContext context) {
  final brightness = Theme.of(context).brightness;
  return brightness == Brightness.dark
      ? AppTokens.darkScaffold
      : AppTokens.lightScaffold;
}

Color secondaryLabel(BuildContext context) {
  return Theme.of(context).brightness == Brightness.dark
      ? AppTokens.secondaryLabelDark
      : AppTokens.secondaryLabelLight;
}
